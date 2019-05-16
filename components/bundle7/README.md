# Bundle Protocol v7 Parser

C implementation of the Bundle Protcol v7 as specified in the [IETF Draft].


## Dependencies

### upcn

The **bundle7** library requires some header files of the **upcn** library. The
header files are currently symlinked directly into the bundle7 include
directory. The only implementation needed from the upcn library is the
`eidManager`.


### Hardware Abstraction Layer

Because upcn uses an hardware abstraction layer (hal) with a custom memory
allocation implementation for the STM32M4 board, we have to include a subset of
the hal header files. Platform specific definitions are in the
`include/{posix,stm32}` directories.


[IETF Draft]: [https://tools.ietf.org/html/draft-ietf-dtn-bpbis-06]
