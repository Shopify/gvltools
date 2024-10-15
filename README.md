# GVLTools

Set of GVL instrumentation tools

## Requirements

GVLTools uses the GVL instrumentation API added in Ruby 3.2.0.

To make it easier to use, on older Rubies the gem will install and expose the same methods, but they won't have any effect and all metrics will report `0`.

## Installation

Install the gem and add to the application's Gemfile by executing:

    $ bundle add gvltools

If bundler is not being used to manage dependencies, install the gem by executing:

    $ gem install gvltools

## Usage

GVLTools expose metric modules with a common interface, for instance `GVLTools::LocalTimer`:

```ruby
GVLTools::LocalTimer.enable
GVLTools::LocalTimer.disable
GVLTools::LocalTimer.enabled? # => true / false
GVLTools::LocalTimer.reset
```

By default, all metrics are disabled.

Note that using the GVL instrumentation API adds some overhead each time Ruby has to switch threads.
In production it is recommended to only enable it on a subset of processes or for small periods of time as to not impact
the average latency too much.

The exact overhead is not yet known, early testing showed a `1-5%` slowdown on a fully saturated multi-threaded app.

### LocalTimer

`LocalTimer` records the overall time spent waiting on the GVL by the current thread.
It is particularly useful to detect wether an application use too many threads, and how much latency is impacted.
For instance as a Rack middleware:

```ruby
# lib/gvl_instrumentation_middleware.rb
class GVLInstrumentationMiddleware
  def initialize(app)
    @app = app
  end

  def call(env)
    before = GVLTools::LocalTimer.monotonic_time
    response = @app.call(env)
    diff = GVLTools::LocalTimer.monotonic_time - before
    puts "Waited #{diff} nanoseconds on the GVL"
    response
  end
end

# config/application.rb
GVLTools::LocalTimer.enable
require "gvl_instrumentation_middleware"
config.middleware.use GVLInstrumentationMiddleware
```

Starting from Ruby 3.3, a thread local timer can be accessed from another thread:

```ruby
def fibonacci(n)
  if n < 2
    n
  else
    fibonacci(n - 1) + fibonacci(n - 2)
  end
end

GVLTools::LocalTimer.enable
thread = Thread.new do
  fibonacci(20)
end
thread.join(1)
local_timer = GVLTools::LocalTimer.for(thread)
local_timer.monotonic_time # => 127000
```

### GlobalTimer

`GlobalTimer` records the overall time spent waiting on the GVL by all threads combined.

```ruby
def fibonacci(number)
  number <= 1 ? number : fibonacci(number - 1) + fibonacci(number - 2)
end

before = GVLTools::GlobalTimer.monotonic_time
threads = 5.times.map do
  Thread.new do
    5.times do
      fibonacci(30)
    end
  end
end
threads.each(&:join)
diff = GVLTools::GlobalTimer.monotonic_time - before
puts "Waited #{(diff / 1_000_000.0).round(1)}ms on the GVL"
```

outputs:

```
Waited 4122.8ms on the GVL
```

### WaitingThreads

`WaitingThreads` records how many threads are currently waiting on the GVL to start executing code.

```ruby
def fibonacci(number)
  number <= 1 ? number : fibonacci(number - 1) + fibonacci(number - 2)
end

Thread.new do
  10.times do
    sleep 0.001
    p GVLTools::WaitingThreads.count
  end
end

threads = 5.times.map do
  Thread.new do
    5.times do
      fibonacci(30)
    end
  end
end
threads.each(&:join)
```

Outputs:

```
5
5
4
```

It's less precise than timers, but the instrumentation overhead is lower.

## Development

After checking out the repo, run `bin/setup` to install dependencies. Then, run `rake test` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and the created tag, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/Shopify/gvltools.

## License

The gem is available as open source under the terms of the [MIT License](https://opensource.org/licenses/MIT).
