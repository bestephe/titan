
#ifndef _IXGBE_FPP_H_
#define _IXGBE_FPP_H_

#include <linux/prefetch.h>

#define FPP_EXPENSIVE(x)	{}					// Just a hint
#define FPP_ISSET(n, i) (n & (1 << i))
#define FPP_SET(n, i) (n | (1 << i))	// Set the ith bit of n
#define FPP_ISSET(n, i) (n & (1 << i))
	
// Prefetch, Save, and Switch
//TODO: this should include a prefetch size as well.  I think the default
// is to prefetch a cacheline.
#define FPP_PSS(addr, label, batch_size) \
do {\
	prefetch(addr); \
	batch_rips[I] = &&label; \
	I = (I + 1) % batch_size; \
	goto *batch_rips[I]; \
} while(0)

//#define foreach(i, n) for(i = 0; i < n; i ++)

//XXX: the kernel already defines get_cycles()
#if 0
long long get_cycles()
{
	unsigned low, high;
	unsigned long long val;
	asm volatile ("rdtsc" : "=a" (low), "=d" (high));
	val = high;
	val = (val << 32) | low;
	return val;
}
#endif

#endif /* _IXGBE_FPP_H_ */

