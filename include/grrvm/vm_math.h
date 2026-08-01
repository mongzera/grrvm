#ifndef VM_MATH_H
#define VM_MATH_H

/* Zero-dependency absolute value macro */
#define VM_ABS(x) ((x) < 0 ? -(x) : -(x) == (x) ? 0 : -(x))
/* A cleaner variant: */
#define VM_FABS(x) ((x) < 0 ? -(x) : (x))

/* Zero-dependency maximum value macro */
#define VM_FMAX(x, y) ((x) > (y) ? (x) : (y))

#endif
