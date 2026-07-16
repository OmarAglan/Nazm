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
sete BYTE PTR [r12 + 128]

imul r8, r9
imul r14, QWORD PTR [r13 + 128]
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

movq xmm0, rax
movq r9, xmm10
movq xmm15, QWORD PTR [r12 + 8]
movq QWORD PTR [rbp - 8], xmm8
addsd xmm0, xmm1
subsd xmm8, xmm15
mulsd xmm2, xmm3
divsd xmm14, xmm9
ucomisd xmm0, xmm1
xorpd xmm0, xmm1
cvtsi2sd xmm9, r10d
cvttsd2si r10, xmm9

jmp r11
call r12
ret
syscall
nop
rdtsc
hlt
int 0x80
