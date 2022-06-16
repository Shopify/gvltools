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
        Thread.new do
          10.times do
            cpu_work
          end
        end
      end.each(&:join)

      GlobalTimer.disable

      refute_predicate GlobalTimer.monotonic_time, :zero?
      thread_global_time = Thread.new { GlobalTimer.monotonic_time }.join.value
      assert_equal GlobalTimer.monotonic_time, thread_global_time
    end

    def test_local_timer
      LocalTimer.enable

      threads = 5.times.map do
        Thread.new do
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
      thread_local_time = Thread.new { LocalTimer.monotonic_time }.join.value
      assert_predicate thread_local_time, :zero?
    end
  end
end
