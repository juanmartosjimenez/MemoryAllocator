//We did not write this code it is taken from http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/Progs/bit-ops.h
#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )
