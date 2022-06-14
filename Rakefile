# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/testtask"

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/test_*.rb"]
  t.warning = true
  t.verbose = true
end

if RUBY_ENGINE == "ruby" && RUBY_VERSION >= "3.2"
  require "rake/extensiontask"

  Rake::ExtensionTask.new("gvltools") do |ext|
    ext.ext_dir = "ext/gvltools"
    ext.lib_dir = "lib/gvltools"
  end
else
  task :compile do
    # noop
  end

  task :clean do
    # noop
  end
end

Rake::Task["test"].enhance(["compile"])

require "rubocop/rake_task"

RuboCop::RakeTask.new

task default: %i[test rubocop]
