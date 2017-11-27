#include "print.h"
#include "init.h"
#include "thread.h"

void kernel_func(void*);

int main(void) {
	put_str("I am kernel\n");
    init_all();
    
    thread_start("kernel_func", 31, kernel_func, "thread ");

	while(1);
	return 0;
}

void kernel_func(void* arg) {
	char* para = arg;
	while (1) {
		put_str(para);
	}
}
