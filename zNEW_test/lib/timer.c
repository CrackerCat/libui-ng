// 2 may 2019
#include <string.h>
#include "timer.h"
#include "timerpriv.h"

// This is based on the algorithm that Go uses for time.Duration.
// Of course, we're not expressing it the same way...
struct timerStringPart {
	char suffix;
	char suffix2;
	int mode;
	uint32_t maxOrMod;
	int precision;
};

enum {
	modeMaxAndStop,
	modeFracModContinue,
};

static const struct timerStringPart parts[] = {
	{ 'n', 's', modeMaxAndStop, 1000, 0 },
	{ 'u', 's', modeMaxAndStop, 1000000, 3 },
	{ 'm', 's', modeMaxAndStop, 1000000000, 6 },
	{ 's', 0, modeFracModContinue, 60, 9 },
	{ 'm', 0, modeFracModContinue, 60, 0 },
	{ 'h', 0, modeFracModContinue, 60, 0 },
	{ 0, 0, 0, 0, 0 },
};

static int fillFracPart(char *buf, int precision, int start, uint64_t *unsec)
{
	int i;
	bool print;
	uint64_t digit;

	print = false;
	for (i = 0; i < precision; i++) {
		digit = *unsec % 10;
		print = print || (digit != 0);
		if (print) {
			buf[start - 1] = "0123456789"[digit];
			start--;
		}
		*unsec /= 10;
	}
	if (print) {
		buf[start - 1] = '.';
		start--;
	}
	return start;
}

static int fillIntPart(char *buf, int start, uint64_t unsec)
{
	if (unsec == 0) {
		buf[start - 1] = '0';
		start--;
		return start;
	}
	while (unsec != 0) {
		buf[start - 1] = "0123456789"[unsec % 10];
		start--;
		unsec /= 10;
	}
	return start;
}

void timerDurationString(timerDuration d, char buf[timerDurationStringLen])
{
	uint64_t unsec;
	bool neg;
	int start;
	const struct timerStringPart *p;

	memset(buf, 0, timerDurationStringLen * sizeof (char));
	start = 32;

	if (d == 0) {
		buf[0] = '0';
		buf[1] = 's';
		return;
	}
	neg = d < 0;
	if (neg) {
		// C99 §6.2.6.2 resticts the possible signed integer representations in C to either sign-magnitude, 1's complement, or 2's complement.
		// Therefore, INT64_MIN will always be either -INT64_MAX or -INT64_MAX - 1, so we can safely do this to see if we need to special-case INT64_MIN as -INT64_MIN cannot be safely represented, or if we can just say -d as that can be safely represented.
		// See also https://stackoverflow.com/questions/29808397/how-to-portably-find-out-minint-max-absint-min
		if (d < -INT64_MAX) {
			// INT64_MIN is -INT64_MAX - 1.
			// This value comes directly from forcing such a value into Go's code; let's just use it.
			memmove(buf, "-2562047h47m16.854775808s", (25 + 1) * sizeof (char));
			return;
		}
		unsec = (uint64_t) (-d);
	} else
		unsec = (uint64_t) d;

	for (p = parts; p->suffix != 0; p++) {
		if (p->mode == modeMaxAndStop && unsec < p->maxOrMod) {
			if (p->suffix2 != 0) {
				buf[start - 1] = p->suffix2;
				start--;
			}
			buf[start - 1] = p->suffix;
			start--;
			start = fillFracPart(buf, p->precision, start, &unsec);
			start = fillIntPart(buf, start, unsec);
			break;
		}
		if (p->mode == modeFracModContinue && unsec != 0) {
			if (p->suffix2 != 0) {
				buf[start - 1] = p->suffix2;
				start--;
			}
			buf[start - 1] = p->suffix;
			start--;
			start = fillFracPart(buf, p->precision, start, &unsec);
			start = fillIntPart(buf, start, unsec % p->maxOrMod);
			unsec /= p->maxOrMod;
			// and move on to the next one
		}
	}

	if (neg) {
		buf[start - 1] = '-';
		start--;
	}
	memmove(buf, buf + start, 33 - start);
}

// portable implementations of 64x64-bit MulDiv(), because:
// - a division intrinsic was not added to Visual Studio until VS2015
// - there does not seem to be a division intrinsic in GCC or clang as far as I can tell
// - there are no 128-bit facilities in macOS as far as I can tell

static void int128FromUint64(uint64_t n, timerprivInt128 *out)
{
	out->neg = false;
	out->high = 0;
	out->low = n;
}

static void int128FromInt64(int64_t n, timerprivInt128 *out)
{
	if (n >= 0) {
		int128FromUint64((uint64_t) n, out);
		return;
	}
	out->neg = true;
	out->high = 0;
	// And now we do the same INT64_MIN/INT64_MAX juggling as above.
	if (n < -INT64_MAX) {
		out->low = ((uint64_t) INT64_MAX) + 1;
		return;
	}
	out->low = (uint64_t) (-n);
}

// references for this part:
// - https://opensource.apple.com/source/Libc/Libc-1272.200.26/gen/nanosleep.c.auto.html
// - https://en.wikipedia.org/wiki/Division_algorithm#Integer_division_(unsigned)_with_remainder

static void int128UAdd(timerprivInt128 *x, const timerprivInt128 *y)
{
	x->high += y->high;
	x->low += y->low;
	if (x->low < y->low)
		x->high++;
}

static void int128USub(timerprivInt128 *x, const timerprivInt128 *y)
{
	x->high -= y->high;
	if (x->low < y->low)
		x->high--;
	x->low -= y->low;
}

static void int128Lsh1(timerprivInt128 *x)
{
	x->high <<= 1;
	if ((x->low & 0x8000000000000000) != 0)
		x->high |= 1;
	x->low <<= 1;
}

static uint64_t int128Bit(const timerprivInt128 *x, int i)
{
	uint64_t which;

	which = x->low;
	if (i >= 64) {
		i -= 64;
		which = x->high;
	}
	return (which >> i) & 1;
}

static int int128UCmp(const timerprivInt128 *x, const timerprivInt128 *y)
{
	if (x->high < y->high)
		return -1;
	if (x->high > y->high)
		return 1;
	if (x->low < y->low)
		return -1;
	if (x->low > y->low)
		return 1;
	return 0;
}

static void int128BitSet(timerprivInt128 *x, int i)
{
	uint64_t bit;

	bit = 1;
	if (i >= 64) {
		i -= 64;
		bit <<= i;
		x->high |= bit;
		return;
	}
	bit <<= i;
	x->low |= bit;
}

static void int128MulDiv64(timerprivInt128 *x, timerprivInt128 *y, timerprivInt128 *z, timerprivInt128 *quot)
{
	bool finalNeg;
	uint64_t x64high, x64low;
	uint64_t y64high, y64low;
	timerprivInt128 add, numer, rem;
	int i;

	finalNeg = false;
	if (x->neg)
		finalNeg = !finalNeg;
	if (y->neg)
		finalNeg = !finalNeg;
	if (z->neg)
		finalNeg = !finalNeg;
	quot->neg = finalNeg;
	// we now treat x, y, and z as unsigned

	// first, multiply x and y into numer
	// this assumes x->high == y->high == 0
	numer.neg = false;
	// the idea is if x = (a * 2^32) + b and y = (c * 2^32) + d, we can express x * y as ((a * 2^32) + b) * ((c * 2^32) + d)...
	x64high = (x->low >> 32) & 0xFFFFFFFF;
	x64low = x->low & 0xFFFFFFFF;
	y64high = (y->low >> 32) & 0xFFFFFFFF;
	y64low = y->low & 0xFFFFFFFF;
	// and we can expand that out to get...
	numer.high = x64high * y64high;		// a * c * 2^64 +
	numer.low = x64low * y64low;			// b * d +
	add.neg = false;
	add.high = x64high * y64low;			// a * d * 2^32 +
	add.low = (add.high & 0xFFFFFFFF) << 32;
	add.high >>= 32;
	int128UAdd(&numer, &add);
	add.high = x64low * y64high;			// b * c * 2^32
	add.low = (add.high & 0xFFFFFFFF) << 32;
	add.high >>= 32;
	int128UAdd(&numer, &add);
	// I did type this all by hand, btw; the idea does come from Apple's implementation, though they explain it a bit more obtusely, and the odd behavior with anding high into low is to avoid looking like I directly copied their code which does the opposite

	// and now long-divide
	// Apple's implementation uses Newton–Raphson division using doubles to store 1/z but I'd rather go with "slow but guaranteed to be accurate"
	// (Apple also rejects quotients > UINT64_MAX; we won't)
	quot->high = 0;
	quot->low = 0;
	rem.neg = false;
	rem.high = 0;
	rem.low = 0;
	for (i = 127; i >= 0; i--) {
		int128Lsh1(&rem);
		rem.low |= int128Bit(&numer, i);
		if (int128UCmp(&rem, z) >= 0) {
			int128USub(&rem, z);
			int128BitSet(quot, i);
		}
	}
}

void timerprivMulDivInt64(int64_t x, int64_t y, int64_t z, timerprivInt128 *quot)
{
	timerprivInt128 a, b, c;

	int128FromInt64(x, &a);
	int128FromInt64(y, &b);
	int128FromInt64(z, &c);
	int128MulDiv64(&a, &b, &c, quot);
}

void timerprivMulDivUint64(uint64_t x, uint64_t y, uint64_t z, timerprivInt128 *quot)
{
	timerprivInt128 a, b, c;

	int128FromUint64(x, &a);
	int128FromUint64(y, &b);
	int128FromUint64(z, &c);
	int128MulDiv64(&a, &b, &c, quot);
}

int64_t timerprivInt128ToInt64(const timerprivInt128 *n, int64_t min, int64_t minCap, int64_t max, int64_t maxCap)
{
	if (n->neg) {
		int64_t ret;

		if (n->high > 0)
			return minCap;
		if (n->low > (uint64_t) INT64_MAX) {
			// we can't safely convert n->low to int64_t
#if INT64_MIN == -INT64_MAX
			// in this case, we can't store -n->low in an int64_t at all!
			// therefore, it must be out of range
			return minCap;
#else
			// in this case, INT64_MIN == -INT64_MAX - 1
			if (n->low > ((uint64_t) INT64_MAX) + 1)
				// we still can't store -n->low in an int64_t
				return minCap;
			// only one option left
			ret = INT64_MIN;
#endif
		} else {
			// -n->low can safely be stored in an int64_t, so do so
			ret = (int64_t) (n->low);
			ret = -ret;
		}
		if (ret < min)
			return minCap;
		return ret;
	}
	if (n->high > 0)
		return maxCap;
	if (n->low > (uint64_t) max)
		return maxCap;
	return (int64_t) (n->low);
}

uint64_t timerprivInt128ToUint64(const timerprivInt128 *n, uint64_t max, uint64_t maxCap)
{
	if (n->neg)
		return 0;
	if (n->high != 0)
		return maxCap;
	if (n->low > maxCap)
		return maxCap;
	return n->low;
}
