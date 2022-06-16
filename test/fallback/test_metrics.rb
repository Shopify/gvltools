# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestMetrics < Minitest::Test
    def global_timer_has_working_interface
      assert_basic_interface(GlobalTimer)
      assert_equal 0, GlobalTimer.monotonic_time
    end

    def local_timer_has_working_interface
      assert_basic_interface(GlobalTimer)
      assert_equal 0, LocalTimer.monotonic_time
    end

    def waiting_threads_has_working_interface
      assert_basic_interface(WaitingThreads)
      assert_equal 0, LocalTimer.count
    end

    private

    def assert_basic_interface(klass)
      klass.enable
      klass.enabled?
      klass.disable
      klass.reset
    end
  end
end
