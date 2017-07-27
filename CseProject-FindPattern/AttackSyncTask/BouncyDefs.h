
#ifndef BOUNCY_DEFS_H
#define BOUNCY_DEFS_H

// Usually, in between windows there are 6 zero bits, but this could be more if there is
// are many consecutive zeroes.
// We are going to assume that some amount of consecutive zeroes is highly rare and thus
// can be used to identify the end of the decryption process.
#define WINDOW_SIZE 6

#if (WINDOW_SIZE == 6)
	#define MIN_ONES_IN_MULT 9
	#define MAX_ZERO_BITS_BETWEEN_WINDOWS 8
	#define ZERO_BIT_SAMPLES_NUM 100
	#define MAX_NOISY_BITS_IN_CONSECUTIVE_ZEROES 3
	#define BUFFERS_IN_WINDOW 64
#endif

#endif
