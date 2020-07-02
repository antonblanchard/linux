/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_POWERPC_QSPINLOCK_H
#define _ASM_POWERPC_QSPINLOCK_H

#include <asm-generic/qspinlock_types.h>

#define _Q_PENDING_LOOPS	(1 << 9) /* not tuned */

#define smp_mb__after_spinlock()   smp_mb()

static __always_inline int queued_spin_is_locked(struct qspinlock *lock)
{
	smp_mb();
	return atomic_read(&lock->val);
}
#define queued_spin_is_locked queued_spin_is_locked

#include <asm-generic/qspinlock.h>

#endif /* _ASM_POWERPC_QSPINLOCK_H */
