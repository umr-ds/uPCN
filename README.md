µPCN - A Free DTN Implementation for Microcontrollers
=====================================================

**Note:** Currently the documentation does not really deserve its name.
However, we are working on improving it over the course of the next releases.

Platforms
---------

µPCN currently supports two platforms:
1. The STM32F4 embedded platform running FreeRTOS
2. All POSIX-compliant operating systems (plus Linux ;-))

Quick Start
-----------

Both platforms can be built, deployed, tested and used in parallel.
To get started with one or both platforms, just follow the subsequent
instructions.

#### Quick Start for STM32F4

For this platform only the STM32F4 embedded system is supported currently.
However, porting to other Cortex-M3/M4 based SoCs should be trivial.

In order to run µPCN on bare metal (e.g. the STM32F4Discovery board)
three steps are necessary after connecting the board via STLink-enabled USB:

1. Install or unpack the following dependencies:
   - The `gcc-arm-none-eabi` toolchain including `newlib`.
   - [STLinkV2](https://github.com/texane/stlink), including the `st-flash`
     tool.
   - A version of FreeRTOS 7, 8 or 9.
   - For debugging and testing you may need OpenOCD, GDB, GNU Expect,
     GCC for you local architecture and the ZeroMQ headers and library.

2. Copy `config.mk.example` to `config.mk` and set all variables according to
   your installation:
   - Set `TOOLCHAIN_STM32` to the prefix for your *arm-none-eabi* toolchain.
   - Set `FREERTOS_PATH` to the path to your FreeRTOS source.
   - Set `ST_FLASH` to the path to your `st-flash` tool.

3. Type `make burn-stm32` to build the project and to burn `upcn.bin` to the
   board attached via USB.

#### Quick Start for POSIX-compliant Operating Systems

µPCN for POSIX provides the option to build for the local system and for
a (embedded) device.

1. Install or unpack the following dependencies:
   - Install the `gcc` toolchain of your local system.
   - If you want to build for another device, install the `gcc`
     toolchain of this device's architecture as well.
   - For debugging and testing you may need OpenOCD, GDB, GNU Expect,
     GCC for you local architecture and the ZeroMQ headers and library.

2. Copy `config.mk.example` to `config.mk` and set all variables according to
   your installation:
   - Set `TOOLCHAIN_POSIX_LOCAL` to the path/prefix for your
     **local `gcc` toolchain**.
     In most cases, the variable can be left empty because the local `gcc` commands
     are in the environment variable.
   - Set `TOOLCHAIN_POSIX_DEV` to the path/prefix
     for your **device `gcc` toolchain**.
     At least a prefix is necessary (see *config.mk.example*), sometimes the
     whole path is needed.
   - Set `POSIX_DEV_IP` and `POSIX_DEV_USERNAME` to
     the *IP address* and *username* of your device.
     This information is needed for transfering the built binaries to the device

3. Type `make run-posix-local` to build and execute µPCN on your local
   machine.

4. Type `deploy-run-posix-dev` to build, transfer and execute µPCN on your
   (embedded) device.


Getting Started with the Implementation
---------------------------------------

The core part of uPCN is located in `./components/upcn/`.
The starting point of the program can be found in
`./components/upcn/src/main.c`, calling init located in
`./components/upcn/src/init.c`.
This file is the best place to familiarize with the implementation.

Testing
-------

Details about tools for testing and the overall testing approaches can be found
in `./doc/testing.md`.
 

License
-------

The code in `./components/upcn`, `./include`, `./components/test/{include,src}` and
`./tools/{upcn_*,rrnd_*,tcpcl,libupcn,bundle7}`
has been developed specifically for µPCN and is released under a
BSD 3-clause license.
The license can be found in `./components/upcn/LICENSE.txt`.

External code
-------------

As an early starting point for the STM32F4 project structure,
we have used the project https://github.com/elliottt/stm32f4/
as a general basis.

All further code taken from 3rd parties is documented within the source files,
along with the respective original URLs and associated licenses.
Generally, 3rd party code is found in `./components/drv`, `./external/tinycbor` 
(git module) and `./components/system/{PLATFORM}/drv`.
