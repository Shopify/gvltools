# frozen_string_literal: true

module StrictWarnings
  def warn(message)
    raise message
  end
end
Warning.singleton_class.prepend(StrictWarnings)

$LOAD_PATH.unshift File.expand_path("../lib", __dir__)
require "gvltools"

require "minitest/autorun"
