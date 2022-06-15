#include <time.h>
#include <ruby.h>
#include <ruby/thread.h>
#include <ruby/atomic.h>

// Metrics
static rb_internal_thread_event_hook_t *gt_hook = NULL;

static unsigned int enabled_mask = 0;
#define TIMER_GLOBAL        1 << 0
#define TIMER_LOCAL         1 << 1

#define ENABLED(metric) enabled_mask & metric

// Common
#define SECONDS_TO_NANOSECONDS (1000 * 1000 * 1000)

static inline void gt_gettime(struct timespec *time) {
    if (clock_gettime(CLOCK_MONOTONIC, time) == -1) {
        rb_sys_fail("clock_gettime");
    }
}

static inline rb_atomic_t gt_time_diff_ns(struct timespec before, struct timespec after) {
    rb_atomic_t total = 0;
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
            RUBY_INTERNAL_THREAD_EVENT_READY | RUBY_INTERNAL_THREAD_EVENT_RESUMED,
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

// GVLTools::Timer

static rb_atomic_t timer_total = 0;
static _Thread_local struct timespec timer_ready_at;

static void gt_thread_callback(rb_event_flag_t event, const rb_internal_thread_event_data_t *event_data, void *user_data) {

    switch(event) {
        case RUBY_INTERNAL_THREAD_EVENT_READY: {
            gt_gettime(&timer_ready_at);
        }
        break;
        case RUBY_INTERNAL_THREAD_EVENT_RESUMED: {
            struct timespec current_time;
            gt_gettime(&current_time);

            RUBY_ATOMIC_ADD(timer_total, gt_time_diff_ns(timer_ready_at, current_time));
        }
        break;
    }
}


static VALUE timer_monotonic_time(VALUE module) {
    return UINT2NUM(timer_total);
}

static VALUE timer_reset(VALUE module) {
    RUBY_ATOMIC_SET(timer_total, 0);
    return Qtrue;
}

void Init_instrumentation(void) {
    VALUE rb_mGVLTools = rb_const_get(rb_cObject, rb_intern("GVLTools"));

    VALUE rb_mNative = rb_const_get(rb_mGVLTools, rb_intern("Native"));
    rb_define_singleton_method(rb_mNative, "enable_metric", gt_enable_metric, 1);
    rb_define_singleton_method(rb_mNative, "disable_metric", gt_disable_metric, 1);
    rb_define_singleton_method(rb_mNative, "metric_enabled?", gt_metric_enabled_p, 1);

    VALUE rb_mGlobalTimer = rb_const_get(rb_mGVLTools, rb_intern("GlobalTimer"));
    rb_define_singleton_method(rb_mGlobalTimer, "reset", timer_reset, 0);
    rb_define_singleton_method(rb_mGlobalTimer, "monotonic_time", timer_monotonic_time, 0);
}
