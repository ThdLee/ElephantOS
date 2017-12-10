#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"

void kernel_func_a(void*);
void kernel_func_b(void*);

int main(void) {
	put_str("I am kernel\n");
    init_all();
    
    // thread_start("kernel_func_a", 31, kernel_func_a, "thread_A ");
    // thread_start("kernel_func_b", 8, kernel_func_b, "thread_B ");

    intr_enable();  // 打开中断，使时钟起作用
	while(1); //{
	// 	console_put_str("Main ");
	// }
	return 0;
}

void kernel_func_a(void* arg) {
	char* para = arg;
	while (1) {
		console_put_str(para);
	}
}

void kernel_func_b(void* arg) {
	char* para = arg;
	while (1) {
		console_put_str(para);
	}
}
