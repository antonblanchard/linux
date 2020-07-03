/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __ASM_SIMPLE_SPINLOCK_H
#define __ASM_SIMPLE_SPINLOCK_H
#ifdef __KERNEL__

/*
 * Simple spin lock operations.  
 *
 * Copyright (C) 2001-2004 Paul Mackerras <paulus@au.ibm.com>, IBM
 * Copyright (C) 2001 Anton Blanchard <anton@au.ibm.com>, IBM
 * Copyright (C) 2002 Dave Engebretsen <engebret@us.ibm.com>, IBM
 *	Rework to support virtual processors
 *
 * Type of int is used as a full 64b word is not necessary.
 *
 * (the type definitions are in asm/simple_spinlock_types.h)
 */
#include <linux/irqflags.h>
#include <asm/synch.h>
#include <asm/ppc-opcode.h>

#define LOCK_TOKEN	1

static __always_inline int arch_spin_value_unlocked(arch_spinlock_t lock)
{
	return lock.slock == 0;
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	smp_mb();
	return !arch_spin_value_unlocked(*lock);
}

/*
 * This returns the old value in the lock, so we succeeded
 * in getting the lock if the return value is 0.
 */
static inline unsigned long __arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned long tmp, token;

	token = LOCK_TOKEN;
	__asm__ __volatile__(
"1:	" PPC_LWARX(%0,0,%2,1) "\n\
	cmpwi		0,%0,0\n\
	bne-		2f\n\
	stwcx.		%1,0,%2\n\
	bne-		1b\n"
	PPC_ACQUIRE_BARRIER
"2:"
	: "=&r" (tmp)
	: "r" (token), "r" (&lock->slock)
	: "cr0", "memory");

	return tmp;
}

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	return __arch_spin_trylock(lock) == 0;
}

static inline void spin_yield(arch_spinlock_t *lock)
{
	barrier();
}

static inline void rw_yield(arch_rwlock_t *lock)
{
	barrier();
}

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	while (1) {
		if (likely(__arch_spin_trylock(lock) == 0))
			break;
		do {
			HMT_low();
		} while (unlikely(lock->slock != 0));
		HMT_medium();
	}
}

static inline
void arch_spin_lock_flags(arch_spinlock_t *lock, unsigned long flags)
{
	unsigned long flags_dis;

	while (1) {
		if (likely(__arch_spin_trylock(lock) == 0))
			break;
		local_save_flags(flags_dis);
		local_irq_restore(flags);
		do {
			HMT_low();
		} while (unlikely(lock->slock != 0));
		HMT_medium();
		local_irq_restore(flags_dis);
	}
}
#define arch_spin_lock_flags arch_spin_lock_flags

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	__asm__ __volatile__("# arch_spin_unlock\n\t"
				PPC_RELEASE_BARRIER: : :"memory");
	lock->slock = 0;
}

/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 */

#define WRLOCK_TOKEN		(-1)

/*
 * This returns the old value in the lock + 1,
 * so we got a read lock if the return value is > 0.
 */
static inline long __arch_read_trylock(arch_rwlock_t *rw)
{
	long tmp;

	__asm__ __volatile__(
"1:	" PPC_LWARX(%0,0,%1,1) "\n"
"	addic.		%0,%0,1\n\
	ble-		2f\n"
"	stwcx.		%0,0,%1\n\
	bne-		1b\n"
	PPC_ACQUIRE_BARRIER
"2:"	: "=&r" (tmp)
	: "r" (&rw->lock)
	: "cr0", "xer", "memory");

	return tmp;
}

/*
 * This returns the old value in the lock,
 * so we got the write lock if the return value is 0.
 */
static inline long __arch_write_trylock(arch_rwlock_t *rw)
{
	long tmp, token;

	token = WRLOCK_TOKEN;
	__asm__ __volatile__(
"1:	" PPC_LWARX(%0,0,%2,1) "\n\
	cmpwi		0,%0,0\n\
	bne-		2f\n"
"	stwcx.		%1,0,%2\n\
	bne-		1b\n"
	PPC_ACQUIRE_BARRIER
"2:"	: "=&r" (tmp)
	: "r" (token), "r" (&rw->lock)
	: "cr0", "memory");

	return tmp;
}

static inline void arch_read_lock(arch_rwlock_t *rw)
{
	while (1) {
		if (likely(__arch_read_trylock(rw) > 0))
			break;
		do {
			HMT_low();
		} while (unlikely(rw->lock < 0));
		HMT_medium();
	}
}

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	while (1) {
		if (likely(__arch_write_trylock(rw) == 0))
			break;
		do {
			HMT_low();
		} while (unlikely(rw->lock != 0));
		HMT_medium();
	}
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	return __arch_read_trylock(rw) > 0;
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	return __arch_write_trylock(rw) == 0;
}

static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	long tmp;

	__asm__ __volatile__(
	"# read_unlock\n\t"
	PPC_RELEASE_BARRIER
"1:	lwarx		%0,0,%1\n\
	addic		%0,%0,-1\n"
"	stwcx.		%0,0,%1\n\
	bne-		1b"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "xer", "memory");
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	__asm__ __volatile__("# write_unlock\n\t"
				PPC_RELEASE_BARRIER: : :"memory");
	rw->lock = 0;
}

#define arch_spin_relax(lock)	spin_yield(lock)
#define arch_read_relax(lock)	rw_yield(lock)
#define arch_write_relax(lock)	rw_yield(lock)

/* See include/linux/spinlock.h */
#define smp_mb__after_spinlock()   smp_mb()

#endif /* __KERNEL__ */
#endif /* __ASM_SIMPLE_SPINLOCK_H */
