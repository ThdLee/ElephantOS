#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "print.h"
#include "debug.h"
#include "process.h"
#include "sync.h"

struct task_struct* main_thread;	// 主线程PCB
struct task_struct* idle_thread;	// idle线程
struct list thread_ready_list;		// 就绪队列
struct list thread_all_list;		// 所有任务队列
struct lock pid_lock;				// 分配pid锁
static struct list_elem* thread_tag;	// 用于保存队列中的线程结点

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);

// 系统空闲时运行的线程
static void idle(void* arg UNUSED) {
	while(1) {
		thread_block(TASK_BLOCKED);
		// 执行hlt时必须要保证目前处在开中断的情况下
		asm volatile ("sti; hlt" : : : "memory");
	}
}

// 获取当前线程pcb指针
struct task_struct* running_thread() {
	uint32_t esp;
	asm ("mov %%esp, %0" : "=g" (esp));
	// 取esp整数部分即pcb起始地址
	return (struct task_struct*)(esp & 0xfffff000); // 页的大小为2的12次方
}

// 由kernel_thread去执行function(func_arg)
static void kernel_thread(thread_func* function, void* func_arg) {
	intr_enable();	// 开中断
	function(func_arg);
}

// 分配pid
static pid_t allocate_pid(void) {
	static pid_t next_pid = 0;
	lock_acquire(&pid_lock);
	next_pid++;
	lock_release(&pid_lock);
	return next_pid;
}

// fork进程时为其分配pid，因为allocate_pid已经是静态的，别的文件无法调用。
pid_t fork_pid(void) {
	return allocate_pid();
}

// 初始化线程栈thread_stack，将待执行的函数和参数放到thread_stack中相应的位置
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
	// 先预留中断使用栈的空间，可见thread.h中定义的结构
	pthread->self_kstack -= sizeof(struct intr_stack);

	// 再留出线程栈空间，可见thread.h中定义
	pthread->self_kstack -= sizeof(struct thread_stack);
	struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
	kthread_stack->eip = kernel_thread;
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

// 初始化线程基本信息
void init_thread(struct task_struct* pthread, char* name, int prio) {
	memset(pthread, 0, sizeof(*pthread));
	pthread->pid = allocate_pid();
	strcpy(pthread->name, name);

	if (pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else {
		pthread->status = TASK_READY;
	}

	// self_kstack是线程自己在内核态下使用的栈顶地址
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
	pthread->priority = prio;
	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;

	// 预留标准输入输出，其余全部置-1
	uint8_t fd_idx = 0;
	while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
		if (fd_idx < 3) {
			pthread->fd_table[fd_idx] = fd_idx;
		} else {
			pthread->fd_table[fd_idx] = -1;
		}
		fd_idx++;
	} 

	pthread->cwd_inode_nr = 0;	// 以根目录作为默认工作目录
	pthread->stack_magic = MAGIC_VALUE;
}

// 创建优先级为prio的线程，名为name，线程执行函数为function(func_arg)
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
	// pcb都位于内核空间，包括用户进程的pcb也是在内核空间
	struct task_struct* thread = get_kernel_pages(1);

	init_thread(thread, name, prio);
	thread_create(thread, function, func_arg);

	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	// 加入就绪队列
	list_append(&thread_ready_list, &thread->general_tag);

	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	// 加入全部线程队列
	list_append(&thread_all_list, &thread->all_list_tag);

	return thread;
}

// 将kernel中的main函数完善为主线程
static void make_main_thread(void) {
	/* 因为main线程在已运行，在loader.S中进入内核时mov esp, 0xc009f000,
	就是为了为其预留tcb，地址为0xc009e000，因此不需要分配页 */
	main_thread = running_thread();
	init_thread(main_thread, "main", 31);

	// main是当前线程，不再就绪队列中
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

// 实现任务调度
void schedule() {

	ASSERT(intr_get_status() == INTR_OFF);

	struct task_struct* cur = running_thread();
	if (cur->status == TASK_RUNNING) {
		ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;	// 重置ticks
		cur->status = TASK_READY;
	} else {

	}

	if (list_empty(&thread_ready_list)) {
		thread_unblock(idle_thread);
	}

	ASSERT(!list_empty(&thread_ready_list));
	thread_tag = NULL;
	// 将就绪队列中的第一个线程弹出，准备将其在cpu上运行
	thread_tag = list_pop(&thread_ready_list);
	struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
	next->status = TASK_RUNNING;

	// 激活任务页表等
	process_activate(next);

	switch_to(cur, next);
}

// 当前线程将自己阻塞
void thread_block(enum task_status status) {
	ASSERT(status == TASK_BLOCKED || status == TASK_WAITING || status == TASK_HANGING);
	enum intr_status old_status = intr_disable();
	struct task_struct* cur_thread = running_thread();
	cur_thread->status = status;	// 重置状态
	schedule();
	// 待当前线程被解除阻塞后才继续运行下面的代码
	intr_set_status(old_status);
}

// 将pthread解除阻塞
void thread_unblock(struct task_struct* pthread) {
	enum intr_status old_status = intr_disable();
	ASSERT(pthread->status == TASK_BLOCKED || pthread->status == TASK_WAITING || pthread->status == TASK_HANGING);
	if (pthread->status != TASK_READY) {
		bool flag = elem_find(&thread_ready_list, &pthread->general_tag);
		ASSERT(!flag);
		if (flag) {
			PANIC("thread_unblock: blocked thread in ready_list\n");
		}
		list_push(&thread_ready_list, &pthread->general_tag);	// 放到队列最前面，使其尽快得到调度
		pthread->status = TASK_READY;
	}
	intr_set_status(old_status);
}

// 主动让出cpu，换其他线程运行
void thread_yield(void) {
	struct task_struct* cur = running_thread();
	enum intr_status old_status = intr_disable();
	ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}

// 初始化线程环境
void thread_init() {
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	lock_init(&pid_lock);

	// 先创建第一个用户进程：init
	process_execute(init, "init");	// 放在第一个初始化，这是第一个进程，pid为1

	// 将当前main函数创建为线程
	make_main_thread();

	// 创建idle线程
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread_init done\n");
}