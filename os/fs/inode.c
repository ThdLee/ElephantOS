#include "inode.h"
#include "fs.h"
#include "file.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "list.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

// 用来存储inode的位置
struct inode_position {
	bool cross_sec;	// inode是否跨扇区
	uint32_t sec_lba;	// inode所在的扇区号
	uint32_t off_size;	// inode在扇区内的字节偏移量
};

// 获取inode所在的扇区和扇区内的偏移量
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
	// inode_table在硬盘上是连续的
	ASSERT(inode_no < MAX_FILES_PER_PART);
	uint32_t inode_table_lba = part->sb->inode_table_lba;

	uint32_t inode_size = sizeof(struct inode);
	uint32_t off_size = inode_no * inode_size;	// 第inode_no号结点相对于inode_table_lba字节偏移量
	uint32_t off_sec = off_size / SECTOR_SIZE;	// 第inode_no号结点相对于inode_table_lba的扇区偏移量
	uint32_t off_size_in_sec = off_size % SECTOR_SIZE;	// 待查找的inode所在扇区中的起始地址

	// 判断此i结点是否跨越两个扇区
	uint32_t left_in_sec = 512 - off_size_in_sec;
	if (left_in_sec < inode_size) {	// 若扇区剩下的空间不足以容纳一个inode，inode必然跨域两个扇区
		inode_pos->cross_sec = true;
	} else {
		inode_pos->cross_sec = false;
	}
	inode_pos->sec_lba = inode_table_lba + off_sec;
	inode_pos->off_size = off_size_in_sec;
}

// 将inode写入到分区part
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {	// io_buf用于硬盘io的缓冲区
	uint8_t inode_no = inode->i_no;
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);		// inode位置信息会存入inode_pos
	ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

	/* 硬盘中inode中的成员inode_tag和i_open_cnts是不需要的，
	 * 它们只在内存中记录链表位置和被多少进程共享 */
	struct inode pure_inode;
	memcpy(&pure_inode, inode, sizeof(struct inode));

	// 以下inode的三个成员只存在于内存中，现在将inode同步到硬盘，清除这三项即可
	pure_inode.i_open_cnts = 0;
	pure_inode.write_deny = false;
	pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

	char* inode_buf = (char*)io_buf;
	if (inode_pos.cross_sec) {
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);

		// 开始讲待写入的inode拼入到2个扇区中相应的位置
		memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));

		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
		memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
	}
}

// 根据i结点号返回相应的i结点
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
	// 先在已打开inode链表中找inode，此链表是为了提速创建的缓冲区
	struct list_elem* elem = part->open_inodes.head.next;
	struct inode* inode_found;
	while (elem != &part->open_inodes.tail) {
		inode_found = elem2entry(struct inode, inode_tag, elem);
		if (inode_found->i_no == inode_no) {
			inode_found->i_open_cnts++;
			return inode_found;
		}
		elem = elem->next;
	}

	// 由于opne_inodes链表中中阿布都，下面从硬盘中读入此inode并键入此链表
	struct inode_position inode_pos;

	// inode位置信息会存入inode_pos，包括inode所在扇区地址和扇区内的字节偏移量
	inode_locate(part, inode_no, &inode_pos);

	/* 为使通过sys_malloc创建新的inode呗所有任务共享，
	 * 需要将inode置于内核空间，故需要临时
	 * 将cur_pbc->pg_dir置位NULL */
	struct task_struct* cur = running_thread();
	uint32_t* cur_pagedir_bak = cur->pgdir;
	cur->pgdir = NULL;

	inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
	// 恢复pgdir
	cur->pgdir = cur_pagedir_bak;

	char* inode_buf;
	if (inode_pos.cross_sec) { 	// 考虑跨扇区的情况
		inode_buf = (char*)sys_malloc(SECTOR_SIZE * 2);
		// i结点表示被partition_format函数连续写入扇区的，所以下面可以连续读出来
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	} else {
		inode_buf = (char*)sys_malloc(SECTOR_SIZE);
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
	}
	memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

	// 因为之后可能会用到此inode，故将其插入到队首便于提前检索到
	list_push(&part->open_inodes, &inode_found->inode_tag);
	inode_found->i_open_cnts = 1;

	sys_free(inode_buf);
	return inode_found;
}

// 关闭inode或减少inode的打开数
void inode_close(struct inode* inode) {
	// 若没有进程再打开此文件，将此inode去掉并释放空间
	enum intr_status old_status = intr_disable();
	if (--inode->i_open_cnts == 0) {
		list_remove(&inode->inode_tag);	// 将结点从part->open_inodes中去掉
		struct task_struct* cur = running_thread();
		uint32_t* cur_pagedir_bak = cur->pgdir;
		cur->pgdir = NULL;
		sys_free(inode);
		cur->pgdir = cur_pagedir_bak;
	}
	intr_set_status(old_status);
}

// 初始化new_inode
void inode_init(uint32_t inode_no, struct inode* new_inode) {
	new_inode->i_no = inode_no;
	new_inode->i_size = 0;
	new_inode->i_open_cnts = 0;
	new_inode->write_deny = false;

	// 初始化索引数组i_sector
	uint8_t sec_idx = 0;
	while (sec_idx < 13) {
		// i_sectors[12]为一级间接块地址
		new_inode->i_sectors[sec_idx] = 0;
		sec_idx++;
	} 
}