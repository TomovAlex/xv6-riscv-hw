
.globl main
.align 4
main:
    addi sp, sp, -160
    sd ra, 152(sp)
    sd s0, 144(sp)
    sd s1, 136(sp)
    sd s2, 128(sp)

    addi s0, sp, 0
    mv a0, s0
    li a1, 128
    call gets

    mv   t0, s0
remove_nl:
    lbu t1, 0(t0)
    beqz t1, remove_nl_done
    li t2, 10
    beq t1, t2, remove_nl_kill
    addi t0, t0, 1
    j remove_nl

remove_nl_kill:
    sb zero, 0(t0)

remove_nl_done:
    la a0, fmt_bar
    mv a1, s0
    call printf
    
    mv t0, s0

skip_spaces:
    lbu t1, 0(t0)
    beqz t1, badfmt
    li t2, ' '
    bne t1, t2, a_start
    addi t0, t0, 1
    j skip_spaces

a_start:
    mv s1, t0

find_space:
    lbu t1, 0(t0)
    beqz t1, badfmt
    li t2, ' '
    beq t1, t2, split
    addi t0, t0, 1
    j find_space

split:
    sb zero, 0(t0)
    addi t0, t0, 1

skip_spaces2:
    lbu t1, 0(t0)
    beqz t1, badfmt
    li t2, ' '
    bne t1, t2, b_start
    addi t0, t0, 1
    j skip_spaces2

b_start:
    mv s2, t0

find_b_end:
    lbu t1, 0(t0)
    beqz t1, skip_spaces3
    li t2, ' '
    beq t1, t2, b_end
    addi t0, t0, 1
    j find_b_end

b_end:
    sb zero, 0(t0)
    addi t0, t0, 1

skip_spaces3:
    lbu t1, 0(t0)
    beqz t1, do_atoi
    li t2, ' '
    bne t1, t2, extra_tail
    addi t0, t0, 1
    j skip_spaces3

extra_tail:
    li a0, 2
    la a1, err_extra
    call fprintf
    li a0, 1
    call exit

do_atoi:
    mv a0, s1
    call atoi
    mv t3, a0

    mv a0, s2
    call atoi
    mv t4, a0

    add t5, t3, t4

    la a0, fmt_int
    mv a1, t5
    call printf

    li a0, 0
    call exit

badfmt:
    li a0, 2
    la a1, err_badfmt
    call fprintf
    li a0, 1
    call exit

    .section .rodata
fmt_bar: .string "|%s|\n"
fmt_int: .string "%d\n"
err_badfmt: .string "bad format\n"
err_extra: .string "extra characters after second number\n"