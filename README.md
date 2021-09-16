> :warning: The Cartesi team keeps working internally on the next version of this repository, following its regular development roadmap. Whenever there's a new version ready or important fix, these are published to the public source tree as new releases.

# Cartesi Machine Emulator

The Cartesi Machine Emulator is the reference off-chain implementation of the Cartesi Machine Specification. It's written in C/C++ with POSIX dependencies restricted to the terminal, process, and memory-mapping facilites. It is distributed as a library and scriptable in the Lua programming language.

The emulator implements RISC-V's RV64IMASU ISA. The letters after RV specify the extension set. This selection corresponds to a 64-bit machine, Integer arithmetic with Multiplication and division, Atomic operations, as well as the optional Supervisor and User privilege levels. In addition, Cartesi Machines support the Sv48 mode of address translation and memory protection.

## Getting Started

### Requirements

- C++ Compiler with support for C++17 (tested with GCC >= 8+ and Clang >= 8.x).
- GNU Make >= 3.81
- Cryptoapp 7.0.0
- GRPC 1.38.0
- Lua 5.3.5
- Boost >= 1.71

Obs: Please note that Apple Clang Version number does not follow upstream LLVM/Clang.

#### Ubuntu 20.04

```
$ apt-get install build-essential automake libtool patchelf cmake pkg-config wget git libreadline-dev libboost-coroutine-dev libboost-context-dev libboost-serialization-dev libboost-filesystem-dev libssl-dev openssl libc-ares-dev zlib1g-dev ca-certificates liblua5.3-dev lua-socket
```
#### MacOS

##### MacPorts
```
sudo port install clang-12 automake boost libtool wget cmake pkgconfig c-ares zlib openssl lua lua-luasocket
```

##### Homebrew
```
brew install llvm@12 automake boost libomp wget cmake pkg-config c-ares zlib openssl lua@5.3 luarocks
luarocks --lua-dir=$(brew --prefix)/opt/lua@5.3 install luasocket
```

For emulator scripts to work it is expected that `lua5.3` binary is available in the system PATH. If operating system/package manager
that you are using provides only `lua` or lua binary named in a different way (e.g. on `Homebrew`), please create symbolic 
link or alias `lua5.3`.

### Build

```bash
$ make submodules
$ make dep
$ make
```

Cleaning:

```bash
$ make depclean
$ make clean
```

### Install

```bash
$ make install
```

## Running Tests

Copy the tests binaries to a directory called `tests` and run: (Eg.: )

```bash
$ make test
```

The default search path for binaries is `machine-emulator/tests`. Alternatively you can specify the binaries path using the `TEST_PATH` variable as in:

```bash
$ make test TEST_PATH=/full/path/to/test/binaries
```

## Linter

We use clang-tidy 12 as the linter.

### Install

#### Ubuntu 20.04

You need to install the package clang-tidy-12 and set it as the default executable with update-alternatives.

```bash
$ apt install clang-tidy-12
$ update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-12 120
```

### Running Lint

```bash
$ make lint -j$(nproc)
```

## Usage

```bash
$ lua5.3 run.lua
```

## Code format

We use clang-format to format the code base.

### Install

#### Ubuntu

You need to install the package clang-format-12 and set is as the default executable with update-alternatives.

```bash
$ apt install clang-format-12
$ update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-12 120
```

### Formatting code

```bash
$ make format
```

### Checking whether the code is formatted

```bash
$ make check-format
```

## Coverage

### Dependencies

#### Ubuntu

If you want to run the GCC-based coverage, you should install the lcov package with the following command.

```bash
$ sudo apt install lcov
```

If you want to run the clang-based coverage, you should install the clang package with the following command.

```bash
$ sudo apt install clang llvm
```

### Compilation

Before running the coverage, you should build the emulator with the flag coverage-toolchain=gcc or coverage-toolchain=clang.
Make sure you run `make clean` to clean up any previous compilation.
For GCC-based coverage run the following command.

```bash
$ make coverage-toolchain=gcc -j$(nproc)
```

For clang-based coverage run the following command.

```bash
$ make coverage-toolchain=clang -j$(nproc)
```

### Running coverage

After building the emulator with coverage enable, you should run the following command.
You need to specify the binaries test path using the `TEST_PATH` and the `CARTESI_TESTS_PATH` variables.
You also need to specify the directory containg the ROM, kernel and rootfs with the `CARTESI_IMAGES_PATH` variable.
For instance:

```bash
$ make coverage-toolchain=gcc coverage-report \
    TEST_PATH=$(realpath ../tests/build) \
    CARTESI_TESTS_PATH=$(realpath ../tests/build) \
    CARTESI_IMAGES_PATH=$(realpath ./src)
```

This command will generate a coverage report in the src directory.
For clang coverage, repeat the same command but with the flag coverage-toolchain=clang.

## Contributing

Thank you for your interest in Cartesi! Head over to our [Contributing Guidelines](CONTRIBUTING.md) for instructions on how to sign our Contributors Agreement and get started with
Cartesi!

Please note we have a [Code of Conduct](CODE_OF_CONDUCT.md), please follow it in all your interactions with the project.

## Authors

* *Diego Nehab*

## License

The machine-emulator repository and all contributions are licensed under
[LGPL 3.0](https://www.gnu.org/licenses/lgpl-3.0.html). Please review our [COPYING](COPYING) file.

