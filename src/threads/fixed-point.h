#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* 17.14 fixed-point representation */
#define F (1 << 14)

/* Convert integer n to fixed point */
#define INT_TO_FP(n) ((n) * F)

/* Convert fixed point x to integer (rounding toward zero) */
#define FP_TO_INT_ZERO(x) ((x) / F)

/* Convert fixed point x to integer (rounding to nearest) */
#define FP_TO_INT_NEAREST(x) ((x) >= 0 ? ((x) + F / 2) / F : ((x) - F / 2) / F)

/* Add fixed point x and y */
#define ADD_FP(x, y) ((x) + (y))

/* Subtract fixed point y from x */
#define SUB_FP(x, y) ((x) - (y))

/* Add fixed point x and integer n */
#define ADD_FP_INT(x, n) ((x) + (n) * F)

/* Subtract integer n from fixed point x */
#define SUB_FP_INT(x, n) ((x) - (n) * F)

/* Multiply fixed point x by fixed point y */
#define MULT_FP(x, y) (((int64_t)(x)) * (y) / F)

/* Multiply fixed point x by integer n */
#define MULT_FP_INT(x, n) ((x) * (n))

/* Divide fixed point x by fixed point y */
#define DIV_FP(x, y) (((int64_t)(x)) * F / (y))

/* Divide fixed point x by integer n */
#define DIV_FP_INT(x, n) ((x) / (n))

#endif /* threads/fixed-point.h */