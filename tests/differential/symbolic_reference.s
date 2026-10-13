# GNU as reference for أشكال_رمزية.نظم: the same program, statement for
# statement, with the same symbol names. @READ_ONLY_SECTION@ is replaced by the
# read-only section directive of the object format under test.
.intel_syntax noprefix

.data
قيمة_محلية: .quad 1
بايت_يزيح: .byte 7
.balign 16
جدول_عناوين: .quad دالة_محلية
عنوان_خارجي: .quad دالة_خارجية
عنوان_قيمة: .quad قيمة_محلية
عنوان_مخزن: .quad مخزن_محلي

@READ_ONLY_SECTION@
ثابت_محلي: .long 2026
.balign 8
عنوان_للقراءة: .quad دالة_لاحقة
عنوان_ثابت: .quad قيمة_خارجية
عنوان_جدول: .quad جدول_عناوين

.bss
.balign 16
مخزن_محلي: .zero 16

.text
.globl بداية_رمزية

بداية_رمزية:
    {disp32} jmp هدف_أمامي
دالة_محلية:
    ret
هدف_أمامي:
    call دالة_محلية
    call دالة_لاحقة
    call دالة_خارجية
    {disp32} jmp هدف_أمامي
    {disp32} je هدف_أمامي
    {disp32} je هدف_أخير
    {disp32} jne هدف_أمامي
    {disp32} jne هدف_أخير
    {disp32} jg هدف_أمامي
    {disp32} jg هدف_أخير
    {disp32} jge هدف_أمامي
    {disp32} jge هدف_أخير
    {disp32} jl هدف_أمامي
    {disp32} jl هدف_أخير
    {disp32} jle هدف_أمامي
    {disp32} jle هدف_أخير
    {disp32} jz هدف_أمامي
    {disp32} jz هدف_أخير
    {disp32} jnz هدف_أمامي
    {disp32} jnz هدف_أخير
    {disp32} js هدف_أمامي
    {disp32} js هدف_أخير
    {disp32} jns هدف_أمامي
    {disp32} jns هدف_أخير
    mov rax, QWORD PTR [rip + قيمة_محلية]
    mov QWORD PTR [rip + قيمة_محلية], rax
    mov eax, DWORD PTR [rip + ثابت_محلي]
    mov r15, QWORD PTR [rip + جدول_عناوين]
    mov rax, QWORD PTR [rip + قيمة_خارجية]
    lea r11, [rip + مخزن_محلي]
    lea rbx, [rip + دالة_محلية]
    lea rdx, [rip + قيمة_خارجية]
    movabs rax, OFFSET دالة_محلية
    movabs r15, OFFSET قيمة_محلية
    movabs rcx, OFFSET مخزن_محلي
    movabs rbx, OFFSET دالة_خارجية
    mov DWORD PTR [rip + قيمة_محلية], eax
    mov r8, QWORD PTR [rip + عنوان_للقراءة]
    {disp32} jmp دالة_خارجية
    {disp32} jne دالة_خارجية
    nop
    nop
    nop
    nop
    nop
دالة_لاحقة:
    ret
هدف_أخير:
    hlt
