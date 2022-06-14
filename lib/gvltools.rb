# frozen_string_literal: true

require_relative "gvltools/version"

module GVLTools
  class Error < StandardError; end

  module AbstractInstrumenter
    def enabled?
      false
    end

    def enable
      false
    end

    def disable
      false
    end

    def reset
      false
    end
  end

  module Timer
    extend AbstractInstrumenter

    class << self
      def monotonic_time
        0
      end
    end
  end
end

begin
  require "gvltools/instrumentation"
rescue LoadError
  # No native ext.
end
