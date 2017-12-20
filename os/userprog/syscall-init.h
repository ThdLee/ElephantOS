#ifndef __USERPROG_SYSYCALLINIT_H
#define __USERPROG_SYSYCALLINIT_H
#include "stdint.h"
void syscall_init(void);
uint32_t sys_getpid(void);
#endif