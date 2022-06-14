# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestTimer < Minitest::Test
    def test_has_working_interface
      Timer.enable
      Timer.enabled?
      Timer.disable
      Timer.reset
      assert_equal 0, Timer.monotonic_time
    end
  end
end
