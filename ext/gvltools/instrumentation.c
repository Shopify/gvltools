#include <time.h>
#include <ruby.h>
#include <ruby/thread.h>
#include <stdatomic.h>

typedef unsigned long long counter_t;

// Metrics
static rb_internal_thread_event_hook_t *gt_hook = NULL;

static unsigned int enabled_mask = 0;
#define TIMER_GLOBAL        1 << 0
#define TIMER_LOCAL         1 << 1
#define WAITING_THREADS     1 << 2

#define ENABLED(metric) (enabled_mask & (metric))

#if __STDC_VERSION__ >= 201112
  #define THREAD_LOCAL_SPECIFIER _Thread_local
#elif defined(__GNUC__) && !defined(RB_THREAD_LOCAL_SPECIFIER_IS_UNSUPPORTED)
  /* note that ICC (linux) and Clang are covered by __GNUC__ */
  #define THREAD_LOCAL_SPECIFIER __thread
#endif

// Common
#define SECONDS_TO_NANOSECONDS (1000 * 1000 * 1000)

static THREAD_LOCAL_SPECIFIER bool was_ready = 0;

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
            RUBY_INTERNAL_THREAD_EVENT_EXITED | RUBY_INTERNAL_THREAD_EVENT_READY | RUBY_INTERNAL_THREAD_EVENT_RESUMED,
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
static THREAD_LOCAL_SPECIFIER counter_t local_timer_total = 0;
static THREAD_LOCAL_SPECIFIER struct timespec timer_ready_at = {0};

static VALUE global_timer_monotonic_time(VALUE module) {
    return ULL2NUM(global_timer_total);
}

static VALUE global_timer_reset(VALUE module) {
    global_timer_total = 0;
    return Qtrue;
}

static VALUE local_timer_monotonic_time(VALUE module) {
    return ULL2NUM(local_timer_total);
}

static VALUE local_timer_reset(VALUE module) {
    local_timer_total = 0;
    return Qtrue;
}

// Thread counts
static THREAD_LOCAL_SPECIFIER counter_t waiting_threads_local_generation = 0;
static _Atomic counter_t waiting_threads_generation = 1;
static _Atomic counter_t waiting_threads_total = 0;

static VALUE waiting_threads_count(VALUE module) {
    return ULL2NUM(waiting_threads_total);
}

static VALUE waiting_threads_reset(VALUE module) {
    waiting_threads_generation++;
    waiting_threads_total = 0;
    return Qtrue;
}

// General callback
static void gt_reset_thread_local_state(void) {
    // MRI can re-use native threads, so we need to reset thread local state,
    // otherwise it will leak from one Ruby thread from another.
    was_ready = false;
    local_timer_total = 0;
}

static void gt_thread_callback(rb_event_flag_t event, const rb_internal_thread_event_data_t *event_data, void *user_data) {
    switch(event) {
        case RUBY_INTERNAL_THREAD_EVENT_STARTED:
        case RUBY_INTERNAL_THREAD_EVENT_EXITED: {
            gt_reset_thread_local_state();
        }
        break;
        case RUBY_INTERNAL_THREAD_EVENT_READY: {
            if (!was_ready) was_ready = true;

            if (ENABLED(WAITING_THREADS)) {
                waiting_threads_local_generation = waiting_threads_generation;
                waiting_threads_total++;
            }

            if (ENABLED(TIMER_GLOBAL | TIMER_LOCAL)) {
                gt_gettime(&timer_ready_at);
            }
        }
        break;
        case RUBY_INTERNAL_THREAD_EVENT_RESUMED: {
            if (!was_ready) break; // In case we registered the hook while some threads were already waiting on the GVL

            if (ENABLED(WAITING_THREADS)) {
                if (waiting_threads_local_generation == waiting_threads_generation) {
                    // We have a small race condition window between the generation check and the decrement
                    waiting_threads_total--;
                }
            }

            if (ENABLED(TIMER_GLOBAL | TIMER_LOCAL)) {
                struct timespec current_time;
                gt_gettime(&current_time);
                counter_t diff = gt_time_diff_ns(timer_ready_at, current_time);

                if (ENABLED(TIMER_LOCAL)) {
                    local_timer_total += diff;
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
    VALUE rb_mGVLTools = rb_const_get(rb_cObject, rb_intern("GVLTools"));

    VALUE rb_mNative = rb_const_get(rb_mGVLTools, rb_intern("Native"));
    rb_define_singleton_method(rb_mNative, "enable_metric", gt_enable_metric, 1);
    rb_define_singleton_method(rb_mNative, "disable_metric", gt_disable_metric, 1);
    rb_define_singleton_method(rb_mNative, "metric_enabled?", gt_metric_enabled_p, 1);

    VALUE rb_mGlobalTimer = rb_const_get(rb_mGVLTools, rb_intern("GlobalTimer"));
    rb_define_singleton_method(rb_mGlobalTimer, "reset", global_timer_reset, 0);
    rb_define_singleton_method(rb_mGlobalTimer, "monotonic_time", global_timer_monotonic_time, 0);

    VALUE rb_mLocalTimer = rb_const_get(rb_mGVLTools, rb_intern("LocalTimer"));
    rb_define_singleton_method(rb_mLocalTimer, "reset", local_timer_reset, 0);
    rb_define_singleton_method(rb_mLocalTimer, "monotonic_time", local_timer_monotonic_time, 0);

    VALUE rb_mWaitingThreads = rb_const_get(rb_mGVLTools, rb_intern("WaitingThreads"));
    rb_define_singleton_method(rb_mWaitingThreads, "reset", waiting_threads_reset, 0);
    rb_define_singleton_method(rb_mWaitingThreads, "count", waiting_threads_count, 0);
}
