#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

// 环形队列
struct ioqueue {
	// 生产者消费者问题
	struct lock lock;
	/* 生产者，缓冲区不满是往里添加数据
	 * 否则就睡眠，此项记录在此缓冲区上睡眠的生产者 */
	struct task_struct* producer;
	/* 消费者，缓冲区不空时从里面拿数据
	 * 否则就睡眠，此项记录在此缓冲区上睡眠的消费者 */
	struct task_struct* consumer;
	char buf[bufsize];	// 缓冲区大小
	int32_t head;		// 队首，数据从队首处写入
	int32_t tail;		// 队尾，数据从队尾处读出
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
#endif