#include "debug.h"
#include "print.h"
#include "interrupt.h"

/* 打印文件名，行号，函数名，条件并使程序悬停 */
void panic_spin(char* filename, int line, const char* func, const char* condition) {
	intr_disable();	// 因为有时候胡单独调用panic_spin，所以在此处关中断。
	put_str("Assertion failed: (");
	put_str(condition);
	put_str("), function ");
	put_str(func);
	put_str(", file ");
	put_str(filename);
	put_str(", line 0x");
	put_int(line);
	put_str(".");
}