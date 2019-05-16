#ifndef BUNDLE7_ASSERT_INCLUDED
#define BUNDLE7_ASSERT_INCLUDED

/**
 * uPCN specific definitions / overwrites
 *
 * If you want to use the bundle7 library in another context without the
 * hardware abstraction layer (hal), you can just remove these includes
 *
 * You can hook in your assertion implemention here. Just make sure you
 * include our own "#define ASSERT" *before* including this header file.
 */
#include <stdlib.h>
#include "hal_defines.h"
#include "upcn/buildFlags.h"

/**
 * Fallback to standard assert.h
 */
#ifndef ASSERT

#include <assert.h>
#define ASSERT(expr) assert(expr)

#endif // ASSERT

#endif // BUNDLE7_ASSERT_INCLUDED
