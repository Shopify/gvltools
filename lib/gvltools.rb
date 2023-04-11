# frozen_string_literal: true

require_relative "gvltools/version"

module GVLTools
  class Error < StandardError; end

  module Native
    class << self
      def enable_metric(_metric)
        false
      end
      alias_method :enable_metric, :enable_metric

      def disable_metric(_metric)
        false
      end
      alias_method :disable_metric, :disable_metric

      def metric_enabled?(_metric)
        false
      end
      alias_method :metric_enabled?, :metric_enabled?
    end
  end

  module AbstractInstrumenter
    TIMER_GLOBAL = 1 << 0
    TIMER_LOCAL = 1 << 1
    WAITING_THREADS = 1 << 2

    def enabled?
      Native.metric_enabled?(metric)
    end

    def enable
      Native.enable_metric(metric)
    end

    def disable
      Native.disable_metric(metric)
    end

    def reset
      false
    end

    private

    def metric
      raise NotImplementedError
    end
  end
  private_constant :AbstractInstrumenter

  module GlobalTimer
    extend AbstractInstrumenter

    class << self
      def monotonic_time
        0
      end
      alias_method :monotonic_time, :monotonic_time

      private

      def metric
        TIMER_GLOBAL
      end
    end
  end

  module LocalTimer
    extend AbstractInstrumenter

    class << self
      def monotonic_time
        0
      end
      alias_method :monotonic_time, :monotonic_time

      private

      def metric
        TIMER_LOCAL
      end
    end
  end

  module WaitingThreads
    extend AbstractInstrumenter

    class << self
      def count
        0
      end
      alias_method :count, :count

      def enable
        unless enabled?
          reset
        end
        super
      end

      def reset
        if enabled?
          raise Error, "can't reset WaitingThreads counter while it is active"
        else
          _reset
        end
      end

      private

      def _reset
      end
      alias_method :_reset, :_reset # to be redefined from C.

      def metric
        WAITING_THREADS
      end
    end
  end

  begin
    require "gvltools/instrumentation"
  rescue LoadError
    # No native ext.
  end

  private_constant :Native
end
