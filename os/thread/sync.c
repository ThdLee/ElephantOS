#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"

// 初始化信号量
void sema_init(struct semaphore* psema, uint8_t value) {
	psema->value = value;	// 为信号量赋值
	list_init(&psema->waiters);	// 初始化信号量等待队列
}

// 初始化锁plock
void lock_init(struct lock* plock) {
	plock->holder = NULL;
	plock->holder_repeat_nr = 0;
	sema_init(&plock->semaphore, 1);
}

// 信号量down操作
void sema_down(struct semaphore* psema) {
	// 关中断保证原子操作
	enum intr_status old_status = intr_disable();
	while(psema->value == 0) {
		bool flag = elem_fine(&psema->waiters, &running_thread()->general_tag);
		ASSERT(!flag);
		// 当前线程不应该已在信号量的wiaters队列中
		if (flag) {
			PANIC("sema_down: thread blocked has been in waiters_list\n");
		}
		// 信号量的值等于0，则当前线程把自己加入该锁放到等待队列，然后阻塞自己
		list_append(&psema->waiters, &running_thread()->general_tag);
		thread_block(TASK_BLOCKED);
	} 
	// 若value位1或被唤醒后，会执行下面的代码，也就是获得了锁
	psema->value--;
	ASSERT(psema->valua == 0);
	// 恢复之前的中断状态
	intr_set_status(old_status);
}

// 信号量up操作
void sema_up(struct semaphore* psema) {
	// 关中断，保证原子操作
	enum intr_status old_status = intr_disable();
	ASSERT(psema->value == 0);
	if (!list_empty(&psema->waiters)) {
		struct task_struct* thread_blocked = elem_to_entry(struct task_struct, general_tag, list_pop(&psema->waiters));
		thread_unblock(thread_blocked);
	}
	psema->value++;
	ASSERT(psema->value == 1);
	// 恢复之前的中断状态
	intr_set_status(old_status);
}

// 获取锁plock
void lock_acquire(struct lock* plock) {
	// 排除自己已经持有锁但并未释放的情况
	if (plock->holder != running_thread()) {
		sema_down(&plock->semaphore);
		plock->holder = running_thread();
		ASSERT(plock->holder_repeat_nr == 0);
		plock->holder_repeat_nr = 1;
	} else {
		plock->holder_repeat_nr++;
	}
}

// 释放锁plock
void lock_release(struct lock* plock) {
	ASSERT(plock->holder == running_thread());
	if (plock->holder_repeat_nr > 1) {
		plock->holder_repeat_nr--;
		return;
	}
	ASSERT(plock->holder_repeat_nr == 1);

	plock->holder = NULL;
	plock->holder_repeat_nr = 0;
	sema_up(&plock->semaphore);
}