#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

// 将位图btmp初始化
void bitmap_init(struct bitmap* btmp) {
	memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

// 判断bit_idx位是否为1，为1则返回1，否则返回0
int bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
	uint32_t byte_idx = bit_idx / 8;
	uint32_t bit_odd = bit_idx % 8;
	return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd)) != 0;
}

// 在位图中申请连续cnt个位，成功则返回其起始位置下标，失败返回-1
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
	uint32_t idx_byte = 0;	// 用于记录空闲位所在的字节
	// 逐字节比较
	while ((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) {
		idx_byte++;
	}

	if (idx_byte >= btmp->btmp_bytes_len) {
		return -1;	//	该内存池中找不到可用空间
	}
	// 若在位图数组范围内的某字节内找到了空闲位，在该字节内逐位比对，返回空闲位的索引
	int idx_bit = 0;
	// 和btmp->bits[idx_byte]这个字节逐位对比
	while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) {
		idx_bit++;
	}

	int bit_idx_start = idx_byte * 8 + idx_bit;
	if (cnt == 1) {
		return bit_idx_start;
	}

	uint32_t bit_left = btmp->btmp_bytes_len * 8 - bit_idx_start;	// 	记录还有多少可以判断
	uint32_t next_bit = bit_idx_start + 1;
	uint32_t count = 1;

	if (bit_left < cnt) {
		return -1;
	}

	bit_idx_start = -1;	// 先将其置为-1，若找不到连续的位置就直接返回
	while (bit_left-- > 0) {
		if (!(bitmap_scan_test(btmp, next_bit))) {	// 若next_bit为0
			count++;
		} else {
			count = 0;
		}
		if (count == cnt) {	// 若找到cnt个空位
			bit_idx_start = next_bit - cnt + 1;
			break;
		}
		next_bit++;
	}
	return bit_idx_start;
}

// 将位图btmp的bit_idx位设置为value
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
	ASSERT((value == 0) || (value == 1));
	uint32_t byte_idx = bit_idx / 8;
	uint32_t bit_odd = bit_idx % 8;

	if (value) {
		btmp->bits[byte_idx] |= BITMAP_MASK << bit_odd;
	} else {
		btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
	}
}