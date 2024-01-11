# frozen_string_literal: true

require "mkmf"
if RUBY_ENGINE == "ruby" &&
   have_header("stdatomic.h") &&
   have_func("rb_internal_thread_add_event_hook", ["ruby/thread.h"]) # 3.1+

  $CFLAGS << " -O3 -Wall "
  have_func("rb_internal_thread_specific_get", "ruby/thread.h") # 3.3+
  create_makefile("gvltools/instrumentation")
else
  File.write("Makefile", dummy_makefile($srcdir).join)
end
