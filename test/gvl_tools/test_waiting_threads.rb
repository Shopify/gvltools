# frozen_string_literal: true

require "test_helper"

module GVLTools
  class TestWaitingThreads < Minitest::Test
    include GVLToolsTestHelper

    def teardown
      WaitingThreads.disable
      WaitingThreads.reset
    end

    def test_waiting_threads
      WaitingThreads.enable
      assert_equal 0, WaitingThreads.count

      threads = []
      begin
        threads = 3.times.map do
          Thread.new do
            loop do
              cpu_work
            end
          end
        end
        threads.each { |t| t.join(0.001) }

        # There's a small race condition here. Sometimes the thread that
        # just got prempted is not yet back waiting on the GVL.
        assert_includes [3, 2], WaitingThreads.count
      ensure
        threads.each(&:kill).each(&:join)
      end

      assert_equal 0, WaitingThreads.count
    end

    def test_reset_while_active
      WaitingThreads.enable
      assert_equal 0, WaitingThreads.count

      count = 4
      threads = 5.times.map do
        Thread.new do
          5.times do
            cpu_work
          end
          count -= 1
          GVLTools::WaitingThreads.reset if count > 0
        end
      end
      threads.each(&:join)

      assert_equal 0, WaitingThreads.count
    end
  end
end
