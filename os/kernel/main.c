#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"

void kernel_func_a(void*);
void kernel_func_b(void*);
void user_prog_a(void);
void user_prog_b(void);
int prog_a_pid = 0, prog_b_pid = 0;

int main(void) {
	put_str("I am kernel\n");
  init_all();
    
  process_execute(user_prog_a, "user_prog_a");
  process_execute(user_prog_b, "user_prog_b");

  intr_enable();  // 打开中断，使时钟起作用
  console_put_str(" main_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  thread_start("k_thread_a", 31, kernel_func_a, "argA ");
  thread_start("k_thread_b", 31, kernel_func_b, "argB ");
	while(1);
	return 0;
}

void kernel_func_a(void* arg) {
	char* para = arg;
  console_put_str(" thread_a_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  console_put_str(" prog_a_pid:0x");
	console_put_int(prog_a_pid);
	console_put_char('\n');
  while(1);
}

void kernel_func_b(void* arg) {
  char* para = arg;
  console_put_str(" thread_b_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  console_put_str(" prog_b_pid:0x");
  console_put_int(prog_b_pid);
  console_put_char('\n');
  while(1);
}

void user_prog_a(void) {
	prog_a_pid = getpid();
	while(1);
}

void user_prog_b(void) {
	prog_b_pid = getpid();
	while(1);
}
