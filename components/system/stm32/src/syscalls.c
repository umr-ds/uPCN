/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2009, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

// Modified for uPCN

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stm32f4xx.h>

#undef errno
extern int errno;
extern int _end;
extern int _heap_end;

extern caddr_t _sbrk(int incr)
{
	static unsigned char *cur_end = NULL, *heap_end;
	unsigned char *prev_end, *tmp;
	if (cur_end == NULL) {
		cur_end = (unsigned char *)&_end;
		heap_end = (unsigned char *)&_heap_end;
	}
	prev_end = cur_end;
	tmp = cur_end + incr;
	if (tmp >= heap_end) {
		errno = 12; // ENOMEM
		return NULL;
	} else {
		cur_end = tmp;
		return (caddr_t)prev_end;
	}
}

extern int link(char *old, char *new)
{
	return -1;
}

extern int _close(int file)
{
	return -1;
}

extern int _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

extern int _isatty(int file)
{
	return 1;
}

extern int _lseek(int file, int ptr, int dir)
{
	return 0;
}

extern int _read(int file, char *ptr, int len)
{
	return 0;
}

extern void _exit(int status)
{
	NVIC_SystemReset();
	for (;;)
		;
}

extern void _kill(int pid, int sig)
{
	if (pid == -1)
		_exit(0);
	return;
}

extern int _getpid(void)
{
	return -1;
}

extern int _write(int file, char *ptr, int len)
{
	return -1;
}
