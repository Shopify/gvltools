# frozen_string_literal: true

require_relative "lib/gvltools/version"

Gem::Specification.new do |spec|
  spec.name = "gvltools"
  spec.version = GVLTools::VERSION
  spec.authors = ["Jean Boussier"]
  spec.email = ["jean.boussier@gmail.com"]

  spec.summary = "Set of GVL instrumentation tools"
  spec.homepage = "https://github.com/Shopify/gvltools"
  spec.license = "MIT"
  spec.required_ruby_version = ">= 2.6.0"

  spec.metadata["allowed_push_host"] = "https://rubygems.org"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = spec.homepage
  spec.metadata["changelog_uri"] = "#{spec.homepage}/blob/master/CHANGELOG.md"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(__dir__) do
    `git ls-files -z`.split("\x0").reject do |f|
      (f == __FILE__) || f.match(%r{\A(?:(?:bin|test|spec|features)/|\.(?:git|travis|circleci)|appveyor)})
    end
  end
  spec.bindir = "exe"
  spec.executables = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions = ["ext/gvltools/extconf.rb"]

  spec.add_development_dependency "rake-compiler"
end
