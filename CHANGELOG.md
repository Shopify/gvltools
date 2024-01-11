## [Unreleased]

- Fixed compatibility with Ruby 3.3.0.

## [0.3.0] - 2023-04-11

- Automatically reset the `WaitingThreads` counter when enabling it (#7).
- Disallow resetting the `WaitingThreads` counter when it is active (#7).

## [0.2.0] - 2023-03-28

- Use C11 atomics instead of MRI's `rb_atomic_t`.

## [0.1.0] - 2022-06-14

- Initial release
