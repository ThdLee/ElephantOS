#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"

void kernel_func_a(void*);
void kernel_func_b(void*);
void user_prog_a(void);
void user_prog_b(void);
int var_a = 0, var_b = 0;

int main(void) {
	put_str("I am kernel\n");
    init_all();
    
    thread_start("kernel_func_a", 31, kernel_func_a, "thread_A ");
    thread_start("kernel_func_b", 31, kernel_func_b, "thread_B ");
    process_execute(user_prog_a, "user_prog_a");
    process_execute(user_prog_b, "user_prog_b");

    intr_enable();  // 打开中断，使时钟起作用
	while(1);
	return 0;
}

void kernel_func_a(void* arg) {
	char* para = arg;
	while (1) {
		console_put_str(" v_a:0x");
		console_put_int(var_a);
	}
}

void kernel_func_b(void* arg) {
	char* para = arg;
	while (1) {
		console_put_str(" v_b:0x");
		console_put_int(var_b);
	}
}

void user_prog_a(void) {
	while(1) {
		var_a++;
	}
}

void user_prog_b(void) {
	while(1) {
		var_b++;
	}
}
