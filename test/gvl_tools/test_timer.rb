# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestTimer < Minitest::Test
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
          10.times do |i|
            fibonacci(i)
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

      5.times.map do
        Thread.new do
          10.times do |i|
            fibonacci(i)
          end
          LocalTimer.monotonic_time
        end
      end.each(&:join).map(&:value)

      LocalTimer.disable
      refute_predicate LocalTimer.monotonic_time, :zero?
    end

    def test_local_timer_init
      thread_local_time = Thread.new { LocalTimer.monotonic_time }.join.value
      assert_predicate thread_local_time, :zero?
    end

    private

    def change(callback)
      before = callback.call
      yield
      callback.call - before
    end

    def duration_us
      before = Process.clock_gettime(Process::CLOCK_MONOTONIC, :nanosecond)
      yield
      Process.clock_gettime(Process::CLOCK_MONOTONIC, :nanosecond) - before
    end

    def fibonacci(number)
      number <= 1 ? number : fibonacci(number - 1) + fibonacci(number - 2)
    end
  end
end
