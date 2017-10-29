TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text
;----------- put_char -------------
;功能描述：把栈中的1个字符写入光标所在处
;----------------------------------
global put_char
put_char:
	pushad			;备份32位寄存器环境（push all double）
	;需要保证gs中位正确的视频段选择子
	;为保险起见，每次打印时都为gs赋值
	mov ax, SELECTOR_VIDEO	;不能直接把立即数送入段寄存器
	mov gs, ax

;;;;;;;;;;;;; 获取当前光标位置
	;先获得高8位
	mov dx, 0x03d4		;索引寄存器
	mov al, 0x0e		;用于提供光标位置的高8位
	out dx, al
	mov dx, 0x03d5		;通过读写数据端口0x3d5来获得或设置光标位置
	in al, dx			;得到了光标位置的高8位
	mov ah, al

	;获取低8位
	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x03d5
	in al, dx

	;将光标存入bx
	mov bx, ax
	;在栈中获取待打印的字符
	mov ecx, [esp + 36] ;pushad压入4*8=32字节，加上主调函数4字节的返回地址，故esp+36

	cmp cl, 0xd 		;CR是0x0d, LF是0x0a
	jz .is_carriage_return
	cmp cl, 0xa
	jz .is_line_feed

	cmp cl, 0x8			;BS(backspace)的ascii码是8
	jz .is_backspace
	jmp .put_other

.is_backspace:
	dec bx
	shl bx, 1			;光标左移1位等于乘2，表示将光标对应显存中偏移的字节

	mov byte [gs:bx], 0x20	;将待删除的字节补为0或者空格皆可
	inc bx
	mov byte [gs:bx], 0x07
	shr bx, 1
	jmp .set_cursor
;;;;;;;;;;;;;;;;;;;;;;;;

.put_other:
	shl bx, 1
	mov [gs:bx], cl		;要打印字符
	inc bx
	mov byte [gs:bx], 0x07	;字符属性
	shr bx, 1			;恢复老的光标值
	inc bx				;下一光标值
	cmp bx, 2000
	jl .set_cursor		;若光标值小于2000，表示未写到
						;显存的最后，则去设置新的光标值
						;若超出屏幕字符数的大小
						;则换行处理
.is_line_feed:			;是换行符LF(\n)
.is_carriage_return:	;是回车符CR(\n)
;如果是CR(\r),只要把光标移到行首就行
	xor dx, dx			;被除数的高16位，
	mov ax, bx			;被除数的低16位
	mov si ,80			;把\n与\r都处理为\n，也就是下一行的行首
	div si
						;以上4行处理\r代码
	sub bx, dx			;获得新一行的行首位置
.is_carriage_return_end:	;回车CR处理结束
	add bx, 80
	cmp bx, 2000
.is_line_feed_end:
	jl .set_cursor

;屏幕范围是 0~24，滚屏的原理是将屏幕的第 1~24 行搬运到第 0~23 行
;再将第24行用空格填充
.roll_screen:			;若超出屏幕大小，开始滚屏
	cld
	mov ecx, 960		;2000-80=1920 个字符要搬运，共 1920*2=3840 个字节
						;一次搬4个字节，共960次
	mov esi, 0xc00b80a0 ;第1行行首
	mov edi, 0xc00b8000 ;第0行行首
	rep movsd

;;;;;;;;;;;将最后一行填充为空白
	mov ebx, 3840		;最后一行首字符的第一个字节偏移 1920*2
	mov ecx, 80			;一行80个字符（160字节），每次清空1个字符
						;（2字节），一共需要移动80次
.cls:
	mov word [gs:ebx], 0x0720	;0x0720是白底黑字空格键
	add ebx, 2
	loop .cls
	mov bx, 1920		;将光标值重置为1920，最后一行首字符

.set_cursor:
	;先设置高8位
	mov dx, 0x03d4		;索引寄存器
	mov al, 0x0e		;用于提供光标位置的高8位
	out dx, al
	mov dx, 0x03d5		;通过读写数据端口0x3d5来获得或设置光标位置
	mov al, bh
	out dx, al

	;获取低8位
	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x03d5
	mov al, bl
	out dx, al
.put_char_done:
	popad
	ret
