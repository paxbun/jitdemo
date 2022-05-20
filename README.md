# JIT demo

![Result on Intel i5-8250U, release mode](./result.png)

A simple example showing how JIT works.

# Supported platforms

* Windows 11 x64
* ~~Linux x64~~ (Not tested)

# Project structure

Miscellaneous directories:

* `cmake`: Contains CMake scripts.

Explore the codes in the following order:

* `expr`: Contains expression tree type definitions and parsing logic.
* `jit`: Contains JIT logic.
* `shell`: Contains a simple shell implementation using the features in the above 2 modules. `main()` is in `shell/main.cxx`. (incomplete)
