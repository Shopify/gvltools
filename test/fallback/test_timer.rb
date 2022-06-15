# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestTimer < Minitest::Test
    def test_has_working_interface
      GlobalTimer.enable
      GlobalTimer.enabled?
      GlobalTimer.disable
      GlobalTimer.reset
      assert_equal 0, GlobalTimer.monotonic_time
    end
  end
end
