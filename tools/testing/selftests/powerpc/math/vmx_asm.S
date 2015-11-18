#include "basic_asm.h"

#POS MUST BE 16 ALIGNED!
#define PUSH_VMX(pos,reg) \
	li	reg,pos; \
	stvx	v20,reg,sp; \
	addi	reg,reg,16; \
	stvx	v21,reg,sp; \
	addi	reg,reg,16; \
	stvx	v22,reg,sp; \
	addi	reg,reg,16; \
	stvx	v23,reg,sp; \
	addi	reg,reg,16; \
	stvx	v24,reg,sp; \
	addi	reg,reg,16; \
	stvx	v25,reg,sp; \
	addi	reg,reg,16; \
	stvx	v26,reg,sp; \
	addi	reg,reg,16; \
	stvx	v27,reg,sp; \
	addi	reg,reg,16; \
	stvx	v28,reg,sp; \
	addi	reg,reg,16; \
	stvx	v29,reg,sp; \
	addi	reg,reg,16; \
	stvx	v30,reg,sp; \
	addi	reg,reg,16; \
	stvx	v31,reg,sp;

#POS MUST BE 16 ALIGNED!
#define POP_VMX(pos,reg) \
	li	reg,pos; \
	lvx	v20,reg,sp; \
	addi	reg,reg,16; \
	lvx	v21,reg,sp; \
	addi	reg,reg,16; \
	lvx	v22,reg,sp; \
	addi	reg,reg,16; \
	lvx	v23,reg,sp; \
	addi	reg,reg,16; \
	lvx	v24,reg,sp; \
	addi	reg,reg,16; \
	lvx	v25,reg,sp; \
	addi	reg,reg,16; \
	lvx	v26,reg,sp; \
	addi	reg,reg,16; \
	lvx	v27,reg,sp; \
	addi	reg,reg,16; \
	lvx	v28,reg,sp; \
	addi	reg,reg,16; \
	lvx	v29,reg,sp; \
	addi	reg,reg,16; \
	lvx	v30,reg,sp; \
	addi	reg,reg,16; \
	lvx	v31,reg,sp;

#Carefull this will 'clobber' vmx (by design)
#Don't call this from C
FUNC_START(load_vmx)
	li	r5,0
	lvx	v20,r5,r3
	addi	r5,r5,16
	lvx	v21,r5,r3
	addi	r5,r5,16
	lvx	v22,r5,r3
	addi	r5,r5,16
	lvx	v23,r5,r3
	addi	r5,r5,16
	lvx	v24,r5,r3
	addi	r5,r5,16
	lvx	v25,r5,r3
	addi	r5,r5,16
	lvx	v26,r5,r3
	addi	r5,r5,16
	lvx	v27,r5,r3
	addi	r5,r5,16
	lvx	v28,r5,r3
	addi	r5,r5,16
	lvx	v29,r5,r3
	addi	r5,r5,16
	lvx	v30,r5,r3
	addi	r5,r5,16
	lvx	v31,r5,r3
	blr
FUNC_END(load_vmx)

#Should be safe from C, only touches r4, r5 and v0,v1,v2
FUNC_START(check_vmx)
	PUSH_BASIC_STACK(32)
	mr r4,r3
	li	r3,1 #assume a bad result
	li	r5,0
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v20
	vmr	v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v21
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v22
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v23
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v24
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v25
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v26
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v27
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v28
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v29
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v30
	vand	v2,v2,v1

	addi	r5,r5,16
	lvx	v0,r5,r4
	vcmpequd.	v1,v0,v31
	vand	v2,v2,v1

	li r5,0
	stvx	v2,r5,sp
	ldx	r0,r5,sp
	cmpdi	r0,0xffffffff
	bne	1f
	li	r3,0
1:	POP_BASIC_STACK(32)
	blr
FUNC_END(check_vmx)

#Safe from C
FUNC_START(test_vmx)
	#r3 holds pointer to where to put the result of fork
	#v20-v31 are non-volatile
	PUSH_BASIC_STACK(512)
	std	r3,40(sp) #Address of varray
	std r4,48(sp) #address of pid
	PUSH_VMX(64, r4)

	bl load_vmx

	li	r0,__NR_fork
	sc
	#Pass the result of fork back to the caller
	ld	r9,48(sp)
	std	r3,0(r9)

	ld r3,40(sp)
	bl check_vmx

	POP_VMX(64,r4)
	POP_BASIC_STACK(512)
	blr
FUNC_END(test_vmx)

#int preempt_vmx(v4si *varray, volatile int *not_ready, int *sentinal)
#On starting will (atomically) decrement not_ready as a signal that the FPU
#has been loaded with varray. Will proceed to check the validity of the FPU
#registers while sentinal is not zero.
FUNC_START(preempt_vmx)
	PUSH_BASIC_STACK(512)
	std r3,32(sp) #v4si *varray
	std r4,40(sp) #volatile int *not_ready
	std r5,48(sp) #int *sentinal
	PUSH_VMX(64,r4)

	bl load_vmx

	#Atomic DEC
	ld r3,40(sp)
1:	lwarx r4,0,r3
	addi r4,r4,-1
	stwcx. r4,0,r3
	bne- 1b

2:	ld r3,32(sp)
	bl check_vmx
	cmpdi r3,0
	bne 3f
	ld r4,48(sp)
	ld r5,0(r4)
	cmpwi r5,0
	bne 2b

3:	POP_VMX(64,r4)
	POP_BASIC_STACK(512)
	blr
FUNC_END(preempt_vmx)
