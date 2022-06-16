# frozen_string_literal: true

require "mkmf"
if RUBY_ENGINE == "ruby" && have_func("rb_internal_thread_add_event_hook", ["ruby/thread.h"])
  $CFLAGS << " -O3 -Wall -std=c99 "
  create_makefile("gvltools/instrumentation")
else
  File.write("Makefile", dummy_makefile($srcdir).join)
end
