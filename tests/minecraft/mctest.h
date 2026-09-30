#pragma once
#include <stdio.h>
#include <stdlib.h>

typedef void (*McTestFn)(void);
void McTestRegister(const char *name, McTestFn fn);

struct McTestReg {
	McTestReg(const char *name, McTestFn fn) { McTestRegister(name, fn); }
};

#define MC_TEST(name) \
	static void name(void); \
	static McTestReg reg_##name(#name, name); \
	static void name(void)

#define MC_CHECK(cond) \
	do { \
		if(!(cond)){ \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			exit(1); \
		} \
	} while(0)

#define MC_CHECK_EQ(a, b) \
	do { \
		long long va_ = (long long)(a), vb_ = (long long)(b); \
		if(va_ != vb_){ \
			fprintf(stderr, "%s:%d: CHECK_EQ failed: %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, #a, va_, #b, vb_); \
			exit(1); \
		} \
	} while(0)

#define MC_CHECK_NEAR(a, b, eps) \
	do { \
		double va_ = (double)(a), vb_ = (double)(b); \
		if(va_ - vb_ > (eps) || vb_ - va_ > (eps)){ \
			fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s (%f) vs %s (%f)\n", __FILE__, __LINE__, #a, va_, #b, vb_); \
			exit(1); \
		} \
	} while(0)
