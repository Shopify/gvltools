# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestTimer < Minitest::Test
    def teardown
      Timer.disable
      Timer.reset
    end

    def test_stuff
      assert_equal 0, Timer.monotonic_time
      Timer.enable

      5.times.map do
        Thread.new do
          10.times do |i|
            fibonacci(i)
          end
        end
      end.each(&:join)

      refute_predicate Timer.monotonic_time, :zero?
    end

    private

    def fibonacci(number)
      number <= 1 ? number : fibonacci(number - 1) + fibonacci(number - 2)
    end
  end
end
