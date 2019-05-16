#ifndef BUNDLE7_ALLOCATORS_H_INCLUDED
#define BUNDLE7_ALLOCATORS_H_INCLUDED


/**
 * -------------------------------
 * uPCN specific memory allocators
 * -------------------------------
 *
 * If you want to use the bundle7 library in another context without
 * the hardware abstraction layer (hal), you can just remove these
 * includes or define your own allocators (see below)
 */
#include "upcn/buildFlags.h"

#include <stdlib.h>
#include <hal_defines.h>


/**
 * ------------------------
 * Default memory allocators
 * ------------------------
 *
 * This are the memory allocation hooks used by the bundle7 library. By default
 * it falls back to <stdlib.h> free() and malloc().
 *
 * Please ensure that you include your own "#define bundle7_free" *before*
 * including this header file.
 */
#ifndef bundle7_free
#define bundle7_free free
#endif

#ifndef bundle7_malloc
#define bundle7_malloc malloc
#endif

#ifndef bundle7_realloc
#define bundle7_realloc realloc
#endif

#endif /* BUNDLE7_ALLOCATORS_H_INCLUDED */
