//SPDX-License-Identifier: GPL-2.0
#include <linux/cgroup.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>

#include "cgroup-internal.h"
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/freezer.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>

/*
 * A cgroup is freezing if any FREEZING flags are set.  FREEZING_SELF is
 * set if "FROZEN" is written to freezer.state cgroupfs file, and cleared
 * for "THAWED".  FREEZING_PARENT is set if the parent freezer is FREEZING
 * for whatever reason.  IOW, a cgroup has FREEZING_PARENT set if one of
 * its ancestors has FREEZING_SELF set.
 */
enum freezer_state_flags {
	CGROUP_FREEZER_ONLINE	= (1 << 0), /* freezer is fully online */
	CGROUP_FREEZING_SELF	= (1 << 1), /* this freezer is freezing */
	CGROUP_FREEZING_PARENT	= (1 << 2), /* the parent freezer is freezing */
	CGROUP_FROZEN		= (1 << 3), /* this and its descendants frozen */

	/* mask for all FREEZING flags */
	CGROUP_FREEZING		= CGROUP_FREEZING_SELF | CGROUP_FREEZING_PARENT,
};

struct freezer {
	struct cgroup_subsys_state	css;
	unsigned int			state;
	unsigned int			oem_freeze_flag;
};

static DEFINE_MUTEX(freezer_mutex);

static inline struct freezer *css_freezer(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct freezer, css) : NULL;
}

static inline struct freezer *task_freezer(struct task_struct *task)
{
	return css_freezer(task_css(task, freezer_cgrp_id));
}

static struct freezer *parent_freezer(struct freezer *freezer)
{
	return css_freezer(freezer->css.parent);
}

/*
 * Propagate the cgroup frozen state upwards by the cgroup tree.
 */
static void cgroup_propagate_frozen(struct cgroup *cgrp, bool frozen)
{
	int desc = 1;

	/*
	 * If the new state is frozen, some freezing ancestor cgroups may change
	 * their state too, depending on if all their descendants are frozen.
	 *
	 * Otherwise, all ancestor cgroups are forced into the non-frozen state.
	 */
	while ((cgrp = cgroup_parent(cgrp))) {
		if (frozen) {
			cgrp->freezer.nr_frozen_descendants += desc;
			if (!test_bit(CGRP_FROZEN, &cgrp->flags) &&
			    test_bit(CGRP_FREEZE, &cgrp->flags) &&
			    cgrp->freezer.nr_frozen_descendants ==
			    cgrp->nr_descendants) {
				set_bit(CGRP_FROZEN, &cgrp->flags);
				cgroup_file_notify(&cgrp->events_file);
				desc++;
			}
		} else {
			cgrp->freezer.nr_frozen_descendants -= desc;
			if (test_bit(CGRP_FROZEN, &cgrp->flags)) {
				clear_bit(CGRP_FROZEN, &cgrp->flags);
				cgroup_file_notify(&cgrp->events_file);
				desc++;
			}
		}
	}
}

/*
 * Revisit the cgroup frozen state.
 * Checks if the cgroup is really frozen and perform all state transitions.
 */
void cgroup_update_frozen(struct cgroup *cgrp)
{
	bool frozen;

	lockdep_assert_held(&css_set_lock);

	/*
	 * If the cgroup has to be frozen (CGRP_FREEZE bit set),
	 * and all tasks are frozen and/or stopped, let's consider
	 * the cgroup frozen. Otherwise it's not frozen.
	 */
	frozen = test_bit(CGRP_FREEZE, &cgrp->flags) &&
		cgrp->freezer.nr_frozen_tasks == __cgroup_task_count(cgrp);

	if (frozen) {
		/* Already there? */
		if (test_bit(CGRP_FROZEN, &cgrp->flags))
			return;

		set_bit(CGRP_FROZEN, &cgrp->flags);
	} else {
		/* Already there? */
		if (!test_bit(CGRP_FROZEN, &cgrp->flags))
			return;

		clear_bit(CGRP_FROZEN, &cgrp->flags);
	}
	cgroup_file_notify(&cgrp->events_file);

	/* Update the state of ancestor cgroups. */
	cgroup_propagate_frozen(cgrp, frozen);
}

/*
 * Increment cgroup's nr_frozen_tasks.
 */
static void cgroup_inc_frozen_cnt(struct cgroup *cgrp)
{
	cgrp->freezer.nr_frozen_tasks++;
}

/*
 * Decrement cgroup's nr_frozen_tasks.
 */
static void cgroup_dec_frozen_cnt(struct cgroup *cgrp)
{
	cgrp->freezer.nr_frozen_tasks--;
	WARN_ON_ONCE(cgrp->freezer.nr_frozen_tasks < 0);
}

/*
 * Enter frozen/stopped state, if not yet there. Update cgroup's counters,
 * and revisit the state of the cgroup, if necessary.
 */
void cgroup_enter_frozen(void)
{
	struct cgroup *cgrp;

	if (current->frozen)
		return;

	spin_lock_irq(&css_set_lock);
	current->frozen = true;
	cgrp = task_dfl_cgroup(current);
	cgroup_inc_frozen_cnt(cgrp);
	cgroup_update_frozen(cgrp);
	spin_unlock_irq(&css_set_lock);
}

/*
 * Conditionally leave frozen/stopped state. Update cgroup's counters,
 * and revisit the state of the cgroup, if necessary.
 *
 * If always_leave is not set, and the cgroup is freezing,
 * we're racing with the cgroup freezing. In this case, we don't
 * drop the frozen counter to avoid a transient switch to
 * the unfrozen state.
 */
void cgroup_leave_frozen(bool always_leave)
{
	struct cgroup *cgrp;

	spin_lock_irq(&css_set_lock);
	cgrp = task_dfl_cgroup(current);
	if (always_leave || !test_bit(CGRP_FREEZE, &cgrp->flags)) {
		cgroup_dec_frozen_cnt(cgrp);
		cgroup_update_frozen(cgrp);
		WARN_ON_ONCE(!current->frozen);
		current->frozen = false;
	}
	spin_unlock_irq(&css_set_lock);

	if (unlikely(current->frozen)) {
		/*
		 * If the task remained in the frozen state,
		 * make sure it won't reach userspace without
		 * entering the signal handling loop.
		 */
		spin_lock_irq(&current->sighand->siglock);
		recalc_sigpending();
		spin_unlock_irq(&current->sighand->siglock);
	}
}

/*
 * Freeze or unfreeze the task by setting or clearing the JOBCTL_TRAP_FREEZE
 * jobctl bit.
 */
static void cgroup_freeze_task(struct task_struct *task, bool freeze)
{
	unsigned long flags;

	/* If the task is about to die, don't bother with freezing it. */
	if (!lock_task_sighand(task, &flags))
		return;

	if (freeze) {
		task->jobctl |= JOBCTL_TRAP_FREEZE;
		signal_wake_up(task, false);
	} else {
		task->jobctl &= ~JOBCTL_TRAP_FREEZE;
		wake_up_process(task);
	}

	unlock_task_sighand(task, &flags);
}

/*
 * Freeze or unfreeze all tasks in the given cgroup.
 */
static void cgroup_do_freeze(struct cgroup *cgrp, bool freeze)
{
	struct css_task_iter it;
	struct task_struct *task;

	lockdep_assert_held(&cgroup_mutex);

	spin_lock_irq(&css_set_lock);
	if (freeze)
		set_bit(CGRP_FREEZE, &cgrp->flags);
	else
		clear_bit(CGRP_FREEZE, &cgrp->flags);
	spin_unlock_irq(&css_set_lock);

	css_task_iter_start(&cgrp->self, 0, &it);
	while ((task = css_task_iter_next(&it))) {
		/*
		 * Ignore kernel threads here. Freezing cgroups containing
		 * kthreads isn't supported.
		 */
		if (task->flags & PF_KTHREAD)
			continue;
		cgroup_freeze_task(task, freeze);
	}
	css_task_iter_end(&it);

	/*
	 * Cgroup state should be revisited here to cover empty leaf cgroups
	 * and cgroups which descendants are already in the desired state.
	 */
	spin_lock_irq(&css_set_lock);
	if (cgrp->nr_descendants == cgrp->freezer.nr_frozen_descendants)
		cgroup_update_frozen(cgrp);
	spin_unlock_irq(&css_set_lock);
}

/*
 * Adjust the task state (freeze or unfreeze) and revisit the state of
 * source and destination cgroups.
 */
void cgroup_freezer_migrate_task(struct task_struct *task,
				 struct cgroup *src, struct cgroup *dst)
{
	lockdep_assert_held(&css_set_lock);

	/*
	 * Kernel threads are not supposed to be frozen at all.
	 */
	if (task->flags & PF_KTHREAD)
		return;

	/*
	 * Adjust counters of freezing and frozen tasks.
	 * Note, that if the task is frozen, but the destination cgroup is not
	 * frozen, we bump both counters to keep them balanced.
	 */
	if (task->frozen) {
		cgroup_inc_frozen_cnt(dst);
		cgroup_dec_frozen_cnt(src);
	}
	cgroup_update_frozen(dst);
	cgroup_update_frozen(src);

	/*
	 * Force the task to the desired state.
	 */
	cgroup_freeze_task(task, test_bit(CGRP_FREEZE, &dst->flags));
}

void cgroup_freezer_frozen_exit(struct task_struct *task)
{
	struct cgroup *cgrp = task_dfl_cgroup(task);

	lockdep_assert_held(&css_set_lock);

	cgroup_dec_frozen_cnt(cgrp);
	cgroup_update_frozen(cgrp);
}

static void freeze_cgroup(struct freezer *freezer)
{
	struct css_task_iter it;
	struct task_struct *task;

	css_task_iter_start(&freezer->css, 0, &it);
	while ((task = css_task_iter_next(&it)))
		freeze_cgroup_task(task);
	css_task_iter_end(&it);
}

static void unfreeze_cgroup(struct freezer *freezer)
{
	struct css_task_iter it;
	struct task_struct *task;
	struct task_struct *g, *p;
	uid_t tmp_uid_val = -1;

	css_task_iter_start(&freezer->css, 0, &it);
	while ((task = css_task_iter_next(&it))) {
		if (task->real_cred)
			tmp_uid_val = task->real_cred->uid.val;
		__thaw_task(task);
	}
	css_task_iter_end(&it);
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		if (p->real_cred &&
			p->real_cred->uid.val == tmp_uid_val)
			__thaw_task(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);
}

void cgroup_freeze(struct cgroup *cgrp, bool freeze)
{
	struct cgroup_subsys_state *css;
	struct cgroup *dsct;
	bool applied = false;

	lockdep_assert_held(&cgroup_mutex);

	/*
	 * Nothing changed? Just exit.
	 */
	if (cgrp->freezer.freeze == freeze)
		return;

	cgrp->freezer.freeze = freeze;

	/*
	 * Propagate changes downwards the cgroup tree.
	 */
	css_for_each_descendant_pre(css, &cgrp->self) {
		dsct = css->cgroup;

		if (cgroup_is_dead(dsct))
			continue;

		if (freeze) {
			dsct->freezer.e_freeze++;
			/*
			 * Already frozen because of ancestor's settings?
			 */
			if (dsct->freezer.e_freeze > 1)
				continue;
		} else {
			dsct->freezer.e_freeze--;
			/*
			 * Still frozen because of ancestor's settings?
			 */
			if (dsct->freezer.e_freeze > 0)
				continue;

			WARN_ON_ONCE(dsct->freezer.e_freeze < 0);
		}

		/*
		 * Do change actual state: freeze or unfreeze.
		 */
		cgroup_do_freeze(dsct, freeze);
		applied = true;
	}

	/*
	 * Even if the actual state hasn't changed, let's notify a user.
	 * The state can be enforced by an ancestor cgroup: the cgroup
	 * can already be in the desired state or it can be locked in the
	 * opposite state, so that the transition will never happen.
	 * In both cases it's better to notify a user, that there is
	 * nothing to wait for.
	 */
	if (!applied)
		cgroup_file_notify(&cgrp->events_file);
}

/**
 * freezer_apply_state - apply state change to a single cgroup_freezer
 * @freezer: freezer to apply state change to
 * @freeze: whether to freeze or unfreeze
 * @state: CGROUP_FREEZING_* flag to set or clear
 *
 * Set or clear @state on @cgroup according to @freeze, and perform
 * freezing or thawing as necessary.
 */
static void freezer_apply_state(struct freezer *freezer, bool freeze,
				unsigned int state)
{
	/* also synchronizes against task migration, see freezer_attach() */
	lockdep_assert_held(&freezer_mutex);

	if (!(freezer->state & CGROUP_FREEZER_ONLINE))
		return;

	if (freeze) {
		if (!(freezer->state & CGROUP_FREEZING))
			atomic_inc(&system_freezing_cnt);
		freezer->state |= state;
		freeze_cgroup(freezer);
	} else {
		bool was_freezing = freezer->state & CGROUP_FREEZING;

		freezer->state &= ~state;

		if (!(freezer->state & CGROUP_FREEZING)) {
			if (was_freezing)
				atomic_dec(&system_freezing_cnt);
			freezer->state &= ~CGROUP_FROZEN;
			unfreeze_cgroup(freezer);
		}
	}
}

/**
 * freezer_change_state - change the freezing state of a cgroup_freezer
 * @freezer: freezer of interest
 * @freeze: whether to freeze or thaw
 *
 * Freeze or thaw @freezer according to @freeze.  The operations are
 * recursive - all descendants of @freezer will be affected.
 */
static void freezer_change_state(struct freezer *freezer, bool freeze)
{
	struct cgroup_subsys_state *pos;

	/*
	 * Update all its descendants in pre-order traversal.  Each
	 * descendant will try to inherit its parent's FREEZING state as
	 * CGROUP_FREEZING_PARENT.
	 */
	mutex_lock(&freezer_mutex);
	freezer->oem_freeze_flag = freeze ? 1 : 0;
	rcu_read_lock();
	css_for_each_descendant_pre(pos, &freezer->css) {
		struct freezer *pos_f = css_freezer(pos);
		struct freezer *parent = parent_freezer(pos_f);

		if (!css_tryget_online(pos))
			continue;
		rcu_read_unlock();

		if (pos_f == freezer)
			freezer_apply_state(pos_f, freeze,
					    CGROUP_FREEZING_SELF);
		else
			freezer_apply_state(pos_f,
					    parent->state & CGROUP_FREEZING,
					    CGROUP_FREEZING_PARENT);

		rcu_read_lock();
		css_put(pos);
	}
	rcu_read_unlock();
	mutex_unlock(&freezer_mutex);
}
void unfreezer_fork(struct task_struct *task)
{
	struct freezer *freezer = NULL;
	if (task_css_is_root(task, freezer_cgrp_id))
		return;

	rcu_read_lock();
	freezer = task_freezer(task);
	rcu_read_unlock();

	if (freezer->oem_freeze_flag != 1)
		return;

	pr_debug("%s:%s(%d)try to unfreeze\n", __func__, task->comm, task->pid);
	freezer_change_state(freezer, 0);
}