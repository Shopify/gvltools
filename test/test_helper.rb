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

module GVLToolsTestHelper
  def before_setup
    @_threads = []
  end

  def after_teardown
    @_threads.each do |thread|
      thread.kill
      thread.join
    end
  end

  private

  def spawn_thread(&block)
    thread = Thread.new(&block)
    @_threads << thread
    thread
  end

  def cpu_work
    fibonacci(20)
  end

  def fibonacci(number)
    number <= 1 ? number : fibonacci(number - 1) + fibonacci(number - 2)
  end
end
