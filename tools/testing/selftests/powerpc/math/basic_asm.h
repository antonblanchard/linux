#include <ppc-asm.h>
#include <asm/unistd.h>

#define LOAD_REG_IMMEDIATE(reg,expr) \
	lis	reg,(expr)@highest;	\
	ori	reg,reg,(expr)@higher;	\
	rldicr	reg,reg,32,31;	\
	oris	reg,reg,(expr)@high;	\
	ori	reg,reg,(expr)@l;

#define PUSH_BASIC_STACK(size) \
	std	2,24(sp); \
	mflr	r0; \
	std	r0,16(sp); \
	mfcr	r0; \
	stw	r0,8(sp); \
	stdu	sp,-size(sp);

#define POP_BASIC_STACK(size) \
	addi	sp,sp,size; \
	ld	2,24(sp); \
	ld	r0,16(sp); \
	mtlr	r0; \
	lwz	r0,8(sp); \
	mtcr	r0; \

