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
          spawn_thread do
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

    def test_reset_active_counter
      GVLTools::WaitingThreads.reset
      GVLTools::WaitingThreads.enable
      assert_raises GVLTools::Error do
        GVLTools::WaitingThreads.reset
      end
    end

    def test_enable_resets_the_counter
      2.times do
        GVLTools::WaitingThreads.enable

        threads = 5.times.map do
          spawn_thread do
            5.times do
              cpu_work
            end

            if GVLTools::WaitingThreads.enabled?
              GVLTools::WaitingThreads.disable
            end
          end
        end

        threads.each(&:join)
        assert_operator 0, :<, GVLTools::WaitingThreads.count
        assert_operator 5, :>=, GVLTools::WaitingThreads.count
      end
    end

    def test_disable_consistency
      GVLTools::WaitingThreads.enable

      threads = 5.times.map do
        spawn_thread do
          5.times do
            cpu_work
          end

          if GVLTools::WaitingThreads.enabled?
            GVLTools::WaitingThreads.disable
            GVLTools::WaitingThreads.enable
          end
        end
      end

      threads.each(&:join)
      assert_equal 0, GVLTools::WaitingThreads.count
    end
  end
end
