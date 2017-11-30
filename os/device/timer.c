#include "timer.h"
#include "io.h"
#include "print.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER0_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

uint32_t ticks;

/* 把操作的计数器counter_no、读写锁属性rwl、计数器模式counter_mode写入模式控制寄存器并赋予初始值counter_value */
static void frequency_set(uint8_t counter_port, \
	uint8_t counter_no, \
	uint8_t rwl, \
	uint8_t counter_mode, \
	uint16_t counter_value) {
	// 向控制字寄存器端口0x43写入控制字
	outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
	// 先写入counter_value的低8位
	outb(counter_port, (uint8_t) counter_value);
	// 再写入高8位
	outb(counter_port, (uint8_t) counter_value >> 8);
}

// 时钟的中断处理函数
static void intr_timer_handler() {
	struct task_struct* cur_thread = running_thread();

	ASSERT(cur_thread->stack_magic == MAGIC_VALUE);

	cur_thread->elapsed_ticks++;	// 记录从线程占用的cpu时间嘀嗒数
	ticks++;	// 总嘀嗒数

	if (cur_thread->ticks == 0)	{	// 若时间片用完，则继续调度
		schedule();
	} else {
		cur_thread->ticks--;
	}
}

void timer_init() {
	put_str("timer_init start\n");
	// 设置8253的定时周期，也就是发中断周期
	frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER0_MODE, COUNTER0_VALUE);
	register_handler(0x20, intr_timer_handler);
	put_str("timer_init done\n");
}