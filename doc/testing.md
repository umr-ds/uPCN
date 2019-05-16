# µPCN Testing Documentation

µPCN relies on intensive testing to ensure system stability as well as project quality. The following information should be taken into account by all project contributors.

There are currently three ways to check for code quality issues and verify the proper function of µPCN:
* Static analysis
* Unit tests
* Integration tests

The tests can be either executed manually, or automatically via a Continuous Integration server. This document summarizes which tests are available and how to execute them.

For patches or pull requests to be accepted, _all_ of the below-listed tests have to return a successful result on _all_ platforms.

## Static Analysis

### Compiler Warnings

Most of the available compiler warnings are turned on automatically in the Makefile. This usually gives already an indication of some obvious problems in new code. To check that there are no problems compiling µPCN for various platforms, the following commands should be run:

```
make clean && make posix-local werror=yes CLA=TCP_LEGACY debug=yes
make clean && make posix-local werror=yes CLA=TCPCL debug=yes
make clean && make stm32 werror=yes type=debug
```

### Linter (clang-tidy)

Clang Tidy is an extensible linter that can be used to check for some typical programming errors. It can be executed for µPCN as follows:

```
make clang-tidy-posix
make clang-tidy-stm32
```

Note that [`bear`](https://github.com/rizsotto/Bear) has to be installed, as a CMake-style `compile_commands.json` file needs to be generated. This file tells `clang-tidy` how (with which flags, etc.) the compiler is executed.

### Stylecheck

The `checkpatch.pl` utility available in the Linux kernel source tree is used for µPCN to enforce compatibility with the project's coding style which is the one used for the Linux kernel. 
The coding style compliance check can be executed by running:

```
make check-style
```

No errors or warnings must be shown.

## Unit Tests

For unit testing, the lightweight [Unity](http://www.throwtheswitch.org/unity/) test framework is used. It provides a simple API and everything necessary to check assertions and generate a test report. Most test cases are available and run for the POSIX as well as the STM32 platform.

The tests are located in `components/test/src` and, including the Unity test framework, are compiled into the µPCN test binary (via `make test`). The test groups to be run have to be specified in `components/test/src/test.c`.

### POSIX

To run all unit tests against the POSIX port of µPCN, a `make` command is provided:

```
make run-unittest-posix-local
```

This will automatically build µPCN plus the tests and execute them. An output similar to the following should be displayed:

```
[...]
......................................

-----------------------
44 Tests 0 Failures 0 Ignored
OK
[Mon Mar 26 14:26:00 2018]: Unittests finished without errors! SUCCESS! (-1) [components/test/src/main.c:72]
```

Furthermore, there is a separate testsuite for the POSIX-specific queue implementation which mimicks FreeRTOS's queue behavior. These tests can be executed as follows:

```
make run-queuetest-posix-local
```

### STM32

First, one needs to configure building for the STM32 platform via `config.mk`, as discussed in the [README](../README.md). Now, tests can be built and flashed to the STM32 board via:

```
make burn-unittest-stm32
```

This uses the `st-flash` utility to upload the tests to the board. They are executed automatically on every boot. The output is provided via the USB Virtual COM Port and should be similar to the one on POSIX, as mentioned above.

## Integration Tests

There are several integration test scenarios which check µPCN's behavior. For the integration tests to work, instances of µPCN and of `upcn_connect` (STM32) or `upcn_netconnect` (POSIX) have to be started.

To execute µPCN, use `make` commands as discussed in the [README](../README.md). The convergence layer adapter should be specified for the POSIX build via adding either `CLA=TCP_LEGACY` (most tests) or `CLA=TCPCL` (tcpcl_test) to the `make` command. If an instance of µPCN is already running or the STM32 board executing µPCN is connected via USB, simply run `make connect` (STM32) or `make netconnect` to start the required ZeroMQ interface to µPCN.

**Example for POSIX:**

Start µPCN in a dedicated terminal:

```
make CLA=TCP_LEGACY run-posix-local
```

Start the ZMQ interface in a separat terminal:

```
make netconnect
```

Now, tests can be run against the `TCP_LEGACY` convergence layer adapter.

### upcn_test

The `upcn_test` tool provides some integration test scenarios, verifying the routing algorithm in µPCN.
The most valuable tests can be run via `make` commands:

```
make test-routing
make test-probabilistic-forwarding
```

Further tests can be run by executing `upcn_test` directly. Usage information as well as a list of valid tests can be obtained via:

```
make tools
./tools/build/upcn_test/upcn_test \
	tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
	test
```

Note that these integration tests are specific to the `TCP_LEGACY` (POSIX) and `USB_OTG` (STM32) convergence layers.

### rrnd_test

The neighbor discovery implementation has its own, Python-based testsuite. Documentation for it can be found in the [specific directory](../tools/rrnd_test/README.md). It should at least be verified that the default test scenario (`make nd-test-scenario/1`) runs without problems and the created `.out` file is valid.

**Example for POSIX:**

First, start µPCN and the ZeroMQ interface as outlined above.
Now, start the neighbor discovery request-reply interface in a dedicated terminal:

```
make nd-remote
```

Start the test scenario in a further terminal:

```
make nd-test-scenario/1
```

After the test has been executed, check the results (i.e. that contact prediction is working):

```
python3 tools/rrnd_test/check_out.py ndtest/$(ls -t ndtest | head -1)
```

Note that the RRND tests are also (still) specific to the `TCP_LEGACY` (POSIX) and `USB_OTG` (STM32) convergence layers.

### tcpcl_test

A simple Python-based test scenario for the TCPCL implementation is available. It simulates a sending and a receiving node, trying to exchange a bundle via µPCN.

First, compile and run µPCN with TCPCL support:

```
make clean && make CLA=TCPCL debug=yes run-posix-local
```

Afterwards, start the "receiver" in another terminal:

```
python3 tools/tcpcl_test/tcpcl_sink.py
```

Finally, start the sending node:

```
python3 tools/tcpcl_test/tcpcl_test.py
```

The test was successful if `tcpcl_sink.py` outputs a received bundle in hexadecimal format.
