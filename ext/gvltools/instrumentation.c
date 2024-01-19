#include <time.h>
#include <ruby.h>
#include <ruby/thread.h>
#include <stdatomic.h>

typedef unsigned long long counter_t;

VALUE rb_cLocalTimer = Qnil;

// Metrics
static rb_internal_thread_event_hook_t *gt_hook = NULL;

static unsigned int enabled_mask = 0;
#define TIMER_GLOBAL        1 << 0
#define TIMER_LOCAL         1 << 1
#define WAITING_THREADS     1 << 2

#define ENABLED(metric) (enabled_mask & (metric))

typedef struct {
    bool was_ready;
    _Atomic counter_t timer_total;
    _Atomic counter_t waiting_threads_ready_generation;
    struct timespec timer_ready_at;
} thread_local_state;

#ifdef HAVE_RB_INTERNAL_THREAD_SPECIFIC_GET // 3.3+
static int thread_storage_key = 0;

static size_t thread_local_state_memsize(const void *data) {
    return sizeof(thread_local_state);
}

static const rb_data_type_t thread_local_state_type = {
    .wrap_struct_name = "GVLTools::ThreadLocal",
    .function = {
        .dmark = NULL,
        .dfree = RUBY_DEFAULT_FREE,
        .dsize = thread_local_state_memsize,
    },
    .flags = RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED,
};

static inline thread_local_state *GT_LOCAL_STATE(VALUE thread, bool allocate) {
    thread_local_state *state = rb_internal_thread_specific_get(thread, thread_storage_key);
    if (!state && allocate) {
        VALUE wrapper = TypedData_Make_Struct(rb_cLocalTimer, thread_local_state, &thread_local_state_type, state);
        rb_thread_local_aset(thread, rb_intern("__gvltools_local_state"), wrapper);
        RB_GC_GUARD(wrapper);
        rb_internal_thread_specific_set(thread, thread_storage_key, state);
    }
    return state;
}

#define GT_EVENT_LOCAL_STATE(event_data, allocate) GT_LOCAL_STATE(event_data->thread, allocate)
#define GT_CURRENT_THREAD_LOCAL_STATE() GT_LOCAL_STATE(rb_thread_current(), true)
#else
#if __STDC_VERSION__ >= 201112
  #define THREAD_LOCAL_SPECIFIER _Thread_local
#elif defined(__GNUC__) && !defined(RB_THREAD_LOCAL_SPECIFIER_IS_UNSUPPORTED)
  /* note that ICC (linux) and Clang are covered by __GNUC__ */
  #define THREAD_LOCAL_SPECIFIER __thread
#endif

static THREAD_LOCAL_SPECIFIER thread_local_state __thread_local_state = {0};
#undef THREAD_LOCAL_SPECIFIER

#define GT_LOCAL_STATE(thread) (&__thread_local_state)
#define GT_EVENT_LOCAL_STATE(event_data, allocate) (&__thread_local_state)
#define GT_CURRENT_THREAD_LOCAL_STATE() (&__thread_local_state)
#endif

// Common
#define SECONDS_TO_NANOSECONDS (1000 * 1000 * 1000)

static inline void gt_gettime(struct timespec *time) {
    if (clock_gettime(CLOCK_MONOTONIC, time) == -1) {
        rb_sys_fail("clock_gettime");
    }
}

static inline counter_t gt_time_diff_ns(struct timespec before, struct timespec after) {
    counter_t total = 0;
    total += (after.tv_nsec - before.tv_nsec);
    total += (after.tv_sec - before.tv_sec) * SECONDS_TO_NANOSECONDS;
    return total;
}

static VALUE gt_metric_enabled_p(VALUE module, VALUE metric) {
    return ENABLED(NUM2UINT(metric)) ? Qtrue : Qfalse;
}

static void gt_thread_callback(rb_event_flag_t event, const rb_internal_thread_event_data_t *event_data, void *user_data);
static VALUE gt_enable_metric(VALUE module, VALUE metric) {
    enabled_mask |= NUM2UINT(metric);
    if (!gt_hook) {
        gt_hook = rb_internal_thread_add_event_hook(
            gt_thread_callback,
            RUBY_INTERNAL_THREAD_EVENT_STARTED | RUBY_INTERNAL_THREAD_EVENT_EXITED | RUBY_INTERNAL_THREAD_EVENT_READY | RUBY_INTERNAL_THREAD_EVENT_RESUMED,
            NULL
        );
    }
    return Qtrue;
}

static VALUE gt_disable_metric(VALUE module, VALUE metric) {
    if (ENABLED(NUM2UINT(metric))) {
        enabled_mask -= NUM2UINT(metric);
    }
    if (!enabled_mask && gt_hook) {
        rb_internal_thread_remove_event_hook(gt_hook);
        gt_hook = NULL;
    }
    return Qtrue;
}

// GVLTools::LocalTimer and GVLTools::GlobalTimer
static _Atomic counter_t global_timer_total = 0;

static VALUE global_timer_monotonic_time(VALUE module) {
    return ULL2NUM(global_timer_total);
}

static VALUE global_timer_reset(VALUE module) {
    global_timer_total = 0;
    return Qtrue;
}

static VALUE local_timer_m_monotonic_time(VALUE module) {
    return ULL2NUM(GT_CURRENT_THREAD_LOCAL_STATE()->timer_total);
}

static VALUE local_timer_m_reset(VALUE module) {
    GT_CURRENT_THREAD_LOCAL_STATE()->timer_total = 0;
    return Qtrue;
}

#ifdef HAVE_RB_INTERNAL_THREAD_SPECIFIC_GET
static VALUE local_timer_for(VALUE module, VALUE thread) {
    GT_LOCAL_STATE(thread, true);
    return rb_thread_local_aref(thread, rb_intern("__gvltools_local_state"));
}

static VALUE local_timer_monotonic_time(VALUE timer) {
    thread_local_state *state;
    TypedData_Get_Struct(timer, thread_local_state, &thread_local_state_type, state);
    return ULL2NUM(state->timer_total);
}

static VALUE local_timer_reset(VALUE timer) {
    thread_local_state *state;
    TypedData_Get_Struct(timer, thread_local_state, &thread_local_state_type, state);
    state->timer_total = 0;
    return Qtrue;
}
#endif

// Thread counts
static _Atomic counter_t waiting_threads_total = 0;
static _Atomic counter_t waiting_threads_current_generation = 1;

static VALUE waiting_threads_count(VALUE module) {
    return ULL2NUM(waiting_threads_total);
}

static VALUE waiting_threads_reset(VALUE module) {
    waiting_threads_current_generation++;
    waiting_threads_total = 0;
    return Qtrue;
}

// General callback
static void gt_thread_callback(rb_event_flag_t event, const rb_internal_thread_event_data_t *event_data, void *user_data) {
    switch(event) {
        case RUBY_INTERNAL_THREAD_EVENT_STARTED: {
            // The STARTED event is triggered from the parent thread with the GVL held
            // so we can allocate the struct.
            GT_EVENT_LOCAL_STATE(event_data, true);
        }
        break;
        case RUBY_INTERNAL_THREAD_EVENT_EXITED: {
#ifndef HAVE_RB_INTERNAL_THREAD_SPECIFIC_GET
            thread_local_state *state = GT_EVENT_LOCAL_STATE(event_data, false);
            if (state) {
                // MRI can re-use native threads, so we need to reset thread local state,
                // otherwise it will leak from one Ruby thread from another.
                state->was_ready = false;
                state->timer_total = 0;
            }
#endif
        }
        break;
        case RUBY_INTERNAL_THREAD_EVENT_READY: {
            thread_local_state *state = GT_EVENT_LOCAL_STATE(event_data, false);
            if (!state) {
                break;
            }
            state->was_ready = true;

            if (ENABLED(WAITING_THREADS)) {
                state->waiting_threads_ready_generation = waiting_threads_current_generation;
                waiting_threads_total++;
            }

            if (ENABLED(TIMER_GLOBAL | TIMER_LOCAL)) {
                gt_gettime(&state->timer_ready_at);
            }
        }
        break;
        case RUBY_INTERNAL_THREAD_EVENT_RESUMED: {
            thread_local_state *state = GT_EVENT_LOCAL_STATE(event_data, true);
            if (!state->was_ready)  {
                break; // In case we registered the hook while some threads were already waiting on the GVL
            }

            if (ENABLED(WAITING_THREADS)) {
                if (state->waiting_threads_ready_generation == waiting_threads_current_generation) {
                    waiting_threads_total--;
                }
            }

            if (ENABLED(TIMER_GLOBAL | TIMER_LOCAL)) {
                struct timespec current_time;
                gt_gettime(&current_time);
                counter_t diff = gt_time_diff_ns(state->timer_ready_at, current_time);

                if (ENABLED(TIMER_LOCAL)) {
                    state->timer_total += diff;
                }

                if (ENABLED(TIMER_GLOBAL)) {
                    global_timer_total += diff;
                }
            }
        }
        break;
    }
}

void Init_instrumentation(void) {
#ifdef HAVE_RB_INTERNAL_THREAD_SPECIFIC_GET // 3.3+
    thread_storage_key = rb_internal_thread_specific_key_create();
#endif

    VALUE rb_mGVLTools = rb_const_get(rb_cObject, rb_intern("GVLTools"));

    VALUE rb_mNative = rb_const_get(rb_mGVLTools, rb_intern("Native"));
    rb_define_singleton_method(rb_mNative, "enable_metric", gt_enable_metric, 1);
    rb_define_singleton_method(rb_mNative, "disable_metric", gt_disable_metric, 1);
    rb_define_singleton_method(rb_mNative, "metric_enabled?", gt_metric_enabled_p, 1);

    VALUE rb_mGlobalTimer = rb_const_get(rb_mGVLTools, rb_intern("GlobalTimer"));
    rb_define_singleton_method(rb_mGlobalTimer, "reset", global_timer_reset, 0);
    rb_define_singleton_method(rb_mGlobalTimer, "monotonic_time", global_timer_monotonic_time, 0);

    rb_global_variable(&rb_cLocalTimer);
    rb_cLocalTimer = rb_const_get(rb_mGVLTools, rb_intern("LocalTimer"));
    rb_undef_alloc_func(rb_cLocalTimer);
    rb_define_singleton_method(rb_cLocalTimer, "reset", local_timer_m_reset, 0);
    rb_define_singleton_method(rb_cLocalTimer, "monotonic_time", local_timer_m_monotonic_time, 0);
#ifdef HAVE_RB_INTERNAL_THREAD_SPECIFIC_GET
    rb_define_singleton_method(rb_cLocalTimer, "for", local_timer_for, 1);
    rb_define_method(rb_cLocalTimer, "reset", local_timer_reset, 0);
    rb_define_method(rb_cLocalTimer, "monotonic_time", local_timer_monotonic_time, 0);
#endif

    VALUE rb_mWaitingThreads = rb_const_get(rb_mGVLTools, rb_intern("WaitingThreads"));
    rb_define_singleton_method(rb_mWaitingThreads, "_reset", waiting_threads_reset, 0);
    rb_define_singleton_method(rb_mWaitingThreads, "count", waiting_threads_count, 0);
}
