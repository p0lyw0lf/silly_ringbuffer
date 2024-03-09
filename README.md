# silly_ringbuffer

A threadsafe, lock-free ringbuffer implemented as a header-only library.

## Usage

See the `tests` folder for usage examples. Provided functions:

- `*_try_{push,pop}`: attempts to push/pop a given series of bytes, returning
  an error code if the ringbuffer is full/empty.
- `*_{push,pop}`: same as the above, only `assert`s that there is no error.
- `*_try_{push,pop}_one`: helper function to push/pop a single element,
  returning an error code if the ringbuffer is full/empty.
- `*_{push,pop}_one`: same as the above, only `assert`s that there is no error.
