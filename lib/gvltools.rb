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
    TIMER_LOCAL  = 1 << 1

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

      def metric
        TIMER_LOCAL
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
