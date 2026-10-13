# The symbols أشكال_رمزية.نظم declares with .خارجي. Both candidates are linked
# with this one object, so every external reference resolves identically.
.intel_syntax noprefix

.text
.balign 16
.globl دالة_خارجية
دالة_خارجية:
    ret

.data
.balign 16
.globl قيمة_خارجية
قيمة_خارجية: .quad 9
