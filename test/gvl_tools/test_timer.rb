# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestTimer < Minitest::Test
    include GVLToolsTestHelper

    def teardown
      GlobalTimer.disable
      LocalTimer.disable
      GlobalTimer.reset
      LocalTimer.reset
    end

    def test_global_timer
      assert_equal 0, GlobalTimer.monotonic_time
      GlobalTimer.enable

      5.times.map do
        spawn_thread do
          10.times do
            cpu_work
          end
        end
      end.each(&:join)

      GlobalTimer.disable

      refute_predicate GlobalTimer.monotonic_time, :zero?
      thread_global_time = spawn_thread { GlobalTimer.monotonic_time }.join.value
      assert_equal GlobalTimer.monotonic_time, thread_global_time
    end

    def test_local_timer
      LocalTimer.enable

      threads = 5.times.map do
        spawn_thread do
          cpu_work
          LocalTimer.monotonic_time
        end
      end
      cpu_work
      timers = threads.each(&:join).map(&:value)

      timers.each do |timer|
        refute_predicate timer, :zero?
      end
    end

    def test_local_timer_init
      thread_local_time = spawn_thread { LocalTimer.monotonic_time }.join.value
      assert_predicate thread_local_time, :zero?
    end

    if RUBY_VERSION >= "3.3"
      def test_local_timer_for_other_thread
        LocalTimer.enable

        threads = 5.times.map do
          spawn_thread do
            cpu_work
            LocalTimer.monotonic_time
          end
        end
        cpu_work
        timers = threads.each(&:join).to_h { |t| [t, t.value] }
        external_timers = threads.to_h { |t| [t, LocalTimer.for(t).monotonic_time] }
        assert_equal timers, external_timers
      end
    else
      def test_local_timer_for_other_thread
        refute_respond_to LocalTimer, :for
      end
    end
  end
end
