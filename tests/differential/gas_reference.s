.intel_syntax noprefix
.text

mov rax, 42
mov r8, 2147483648
mov r9, r15
mov r10, QWORD PTR [r12]
mov QWORD PTR [r13 + 128], r11
push r15
pop r8
lea r14, [rsp + 127]

add r8, r9
add r10, -128
add r11, 128
add r12, QWORD PTR [r13 + 128]
sub r13, r14
and r15, -129
or rax, QWORD PTR [rbp]
xor rbx, r12
cmp r9, 127

imul r8, r9
imul r10, r11, -129
idiv r12
inc r13
dec r14
neg r15
not rax
test r8, r9
test r10, 128
shl r8, 1
shr r9, 2
sar r10, cl

jmp r11
call r12
ret
syscall
nop
hlt
int 0x80
