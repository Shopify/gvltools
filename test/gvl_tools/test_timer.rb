# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestTimer < Minitest::Test
    def teardown
      GlobalTimer.disable
      GlobalTimer.reset
    end

    def test_stuff
      assert_equal 0, GlobalTimer.monotonic_time
      GlobalTimer.enable

      5.times.map do
        Thread.new do
          10.times do |i|
            fibonacci(i)
          end
        end
      end.each(&:join)

      refute_predicate GlobalTimer.monotonic_time, :zero?
    end

    private

    def fibonacci(number)
      number <= 1 ? number : fibonacci(number - 1) + fibonacci(number - 2)
    end
  end
end
