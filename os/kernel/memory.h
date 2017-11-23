#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"

enum pool_flags {
	PF_KERNEL = 1,	// 内核存储池
	PF_USER = 2		// 用户存储池
};

#define PG_P_1 1  // 存在
#define PG_P_0 0  // 不存在
#define PG_RW_R 0 // R/W 读/执行
#define PG_RW_W 2 // R/W 读/写/执行
#define PG_US_S 0 // U/S 系统级
#define PG_US_U 4 // U/S 用户级

// 用于虚拟地址管理
struct virtual_addr {
	struct bitmap vaddr_bitmap;	// 虚拟地址用到的位图结构
	uint32_t vaddr_start;		// 虚拟地址起始地址
};

extern struct pool kernel_pool, user_pool;
void mem_init(void);
void* get_kernel_pages(uint32_t pg_cnt);
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
void malloc_init(void);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
#endif