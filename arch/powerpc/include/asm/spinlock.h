/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H
#ifdef __KERNEL__

#include <asm/paravirt.h>
#include <asm/qspinlock.h>
#include <asm/qrwlock.h>
#ifdef CONFIG_PPC_PSERIES
#include <asm/paca.h>
#endif
#include <asm/synch.h>

static __always_inline int powerpc_queued_spin_is_locked(struct qspinlock *lock)
{
	smp_mb();
	return queued_spin_is_locked(lock);
}
#define queued_spin_is_locked powerpc_queued_spin_is_locked

/* See include/linux/spinlock.h */
#define smp_mb__after_spinlock()   smp_mb()

#endif /* __KERNEL__ */
#endif /* __ASM_SPINLOCK_H */
