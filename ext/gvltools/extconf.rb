# frozen_string_literal: true

require "mkmf"
# required_ruby_version (>= 3.3.0) guarantees the 3.3+ internal thread APIs on
# CRuby, so we only need to rule out other engines and missing stdatomic support.
if RUBY_ENGINE == "ruby" && have_header("stdatomic.h")
  $CFLAGS << " -O3 -Wall "
  create_makefile("gvltools/instrumentation")
else
  File.write("Makefile", dummy_makefile($srcdir).join)
end
