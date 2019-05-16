/* This file handles possible uPCN build flags. */

#ifndef UPCNFLAGS_H_INCLUDED
#define UPCNFLAGS_H_INCLUDED

/*
 * The board for which we want to compile, default STM32F4DISCOVERY.
 */

#ifndef BOARD_SOMP2
#ifndef BOARD_STM32F4_DISCOVERY
#define BOARD_STM32F4_DISCOVERY
#endif /* BOARD_STM32F4_DISCOVERY */
#endif /* BOARD_SOMP2 */

/*
 * Use STM SHA1 implementation for the SOMP2 board.
 */

#ifdef BOARD_SOMP2
#define USE_STM_SHA1
#endif /* BOARD_SOMP2 */

/*
 * The test tool does not want memory debugging and/or logging.
 */

#ifdef UPCN_LOCAL
#define NO_MEMDEBUG
#define NO_LOGGING
#define QUIET
#endif /* UPCN_LOCAL */

/*
 * For local builds the bord firmware is not needed.
 */

#ifndef UPCN_LOCAL
#define INCLUDE_BOARD_LIB
#endif /* UPCN_LOCAL */

/*
 * VERBOSE is default for DEBUG builds.
 */
#ifdef DEBUG
#ifndef VERBOSE
#define VERBOSE
#endif /* VERBOSE */
#endif /* DEBUG */

/*
 * Undef overridden defines.
 */

#ifdef NO_MEMDEBUG
#ifdef MEMDEBUG
#undef MEMDEBUG
#endif /* MEMDEBUG */
#endif /* NO_MEMDEBUG */

#ifdef NO_MEMDEBUG
#ifdef MEMDEBUG_STRICT
#undef MEMDEBUG_STRICT
#endif /* MEMDEBUG_STRICT */
#endif /* NO_MEMDEBUG */

#ifdef NO_LOGGING
#ifdef LOGGING
#undef LOGGING
#endif /* LOGGING */
#endif /* NO_LOGGING */

#ifdef QUIET
#ifdef VERBOSE
#undef VERBOSE
#endif /* VERBOSE */
#endif /* QUIET */

/*
 * MEMDEBUG_STRICT implies MEMDEBUG.
 */

#ifdef MEMDEBUG_STRICT
#ifndef MEMDEBUG
#define MEMDEBUG
#endif /* MEMDEBUG */
#endif /* MEMDEBUG_STRICT */

/*
 * Enable advanced communication (bundle_print, ...)
 * for DEBUG and VERBOSE builds.
 */

#ifdef DEBUG
#ifndef UPCN_LOCAL
#define INCLUDE_ADVANCED_COMM
#endif /* UPCN_TEST_TOOL */
#endif /* DEBUG */

#ifdef VERBOSE
#ifndef INCLUDE_ADVANCED_COMM
#define INCLUDE_ADVANCED_COMM
#endif /* INCLUDE_ADVANCED_COMM */
#endif /* VERBOSE */

/*
 * Activate MEMDEBUG and LOGGING per default if DEBUG is defined
 * and they are not disabled.
 */

#ifdef DEBUG

#ifndef NO_MEMDEBUG
#ifndef MEMDEBUG
#define MEMDEBUG
#endif /* MEMDEBUG */
#endif /* NO_MEMDEBUG */

#ifndef NO_LOGGING
#ifndef LOGGING
#define LOGGING
#endif /* LOGGING */
#endif /* NO_LOGGING */

#endif /* DEBUG */

/*
 * Define ASSERT() macro for non-local DEBUG builds.
 */

#ifdef DEBUG
#define INCLUDE_ASSERT
#endif /* DEBUG */

/* Activate debug file tracking if MEMDEBUG or LOGGING are defined. */

#ifdef MEMDEBUG
#ifndef INCLUDE_FILETRACK
#define INCLUDE_FILETRACK
#endif /* INCLUDE_FILETRACK */
#endif /* MEMDEBUG */

#ifdef LOGGING
#ifndef INCLUDE_FILETRACK
#define INCLUDE_FILETRACK
#endif /* INCLUDE_FILETRACK */
#endif /* LOGGING */

#endif /* UPCNFLAGS_H_INCLUDED */
