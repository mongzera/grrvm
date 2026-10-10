#ifndef GRRVM_VM_MATH_H
#define GRRVM_VM_MATH_H

/* Zero-dependency absolute value macros (evaluate x more than once) */
#define VM_FABS(x) ((x) < 0 ? -(x) : (x))
#define VM_ABS(x)  VM_FABS(x)

/* Zero-dependency maximum value macro */
#define VM_FMAX(x, y) ((x) > (y) ? (x) : (y))

#endif /* GRRVM_VM_MATH_H */
