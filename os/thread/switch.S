[bits 32]
section .text
global switch_to
;void switch_to(struct task_struct* cur, struct task_struct* next);
switch_to:
	;栈中此处是返回地址
	push esi
	push edi
	push ebx
	push ebp

	mov eax, [esp + 20]	;cur
	mov [eax], esp

	mov eax, [esp + 24] ;next
	mov esp, [eax]

	pop ebp
	pop ebx
	pop edi
	pop esi
	ret