#include "upcn/upcn.h"

int testupcn_putc(int c)
{
#ifdef ARCHITECTURE_STM32
	hal_io_write_raw(&c, 1);
#else
	putc(c, stdout);
#endif
	return 0;
}
