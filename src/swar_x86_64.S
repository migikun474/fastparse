/* fp_u64_asm: fp_u64_swar (src/parse_swar.c) written by hand. x86-64, AT&T syntax, System V ABI.
 *   in:  rdi = p, rsi = n, rdx = out        out: eax = n, or -fp_status
 * Same steps as the C version: per chunk, load 8 bytes, shift the k digits to the top and fill
 * below with '0', check all 8 are digits, then combine pairs/quads/eight with imul+shr+and.
 * Registers: rdi cursor, rbx end, rsi n, r8 value, r9 out, r10 overflow, rcx k then shift,
 * rax chunk, rdx/r11 scratch (mul writes rdx:rax). */

        .set FP_EMPTY, 1                /* must match fp_status in include/fastparse.h */
        .set FP_BAD_CHAR, 2
        .set FP_OVERFLOW, 3

        .text
        .globl  fp_u64_asm
        .type   fp_u64_asm, @function
fp_u64_asm:
        test    %rsi, %rsi
        jz      .Lempty
        push    %rbx                    /* callee-saved: holds the end pointer */
        mov     %rdx, %r9               /* out; rdx is needed by mul */
        lea     (%rdi,%rsi), %rbx       /* end = p + n */
        xor     %r8d, %r8d              /* value = 0 */
        xor     %r10d, %r10d            /* overflow = 0 */
        lea     -1(%rsi), %rcx
        and     $7, %ecx
        inc     %ecx                    /* k = (n - 1) % 8 + 1 */

.Lchunk:
        mov     (%rdi), %rax            /* 8 bytes; up to 7 past the field (FP_PAD) */
        add     %rcx, %rdi              /* cursor += k */
        mov     $8, %r11d
        sub     %ecx, %r11d
        lea     0(,%r11,8), %ecx        /* shift = 8 * (8 - k) */
        shl     %cl, %rax               /* digits to the top bytes */
        mov     $1, %r11d
        shl     %cl, %r11
        dec     %r11                    /* (1 << shift) - 1 */
        movabs  $0x3030303030303030, %rdx
        and     %rdx, %r11
        or      %r11, %rax              /* '0' fill below the digits */

        movabs  $0xF0F0F0F0F0F0F0F0, %rdx
        mov     %rax, %r11
        and     %rdx, %r11              /* high nibbles */
        movabs  $0x0606060606060606, %rcx
        add     %rax, %rcx
        and     %rdx, %rcx
        shr     $4, %rcx                /* high nibbles of byte + 6, moved down */
        or      %rcx, %r11
        movabs  $0x3333333333333333, %rdx
        cmp     %rdx, %r11
        jne     .Lbad_char              /* a non-digit byte */

        movabs  $0x3030303030303030, %rdx
        sub     %rdx, %rax              /* ASCII to digit values */
        imul    $10, %rax, %r11
        shr     $8, %rax
        add     %r11, %rax
        movabs  $0x00FF00FF00FF00FF, %rdx
        and     %rdx, %rax              /* pairs: 10 * d[i] + d[i+1] */
        imul    $100, %rax, %r11
        shr     $16, %rax
        add     %r11, %rax
        movabs  $0x0000FFFF0000FFFF, %rdx
        and     %rdx, %rax              /* quads */
        imul    $10000, %rax, %r11
        shr     $32, %rax
        add     %r11, %rax
        mov     %eax, %eax              /* eight digits; upper half cleared */

        mov     %rax, %r11              /* value = value * 10^8 + chunk, unsigned */
        mov     $100000000, %eax
        mul     %r8                     /* rdx:rax = value * 10^8 */
        or      %rdx, %r10              /* high half set: overflow */
        add     %r11, %rax
        adc     $0, %r10                /* carry: overflow */
        mov     %rax, %r8
        mov     $8, %ecx                /* every later chunk is a full 8 */
        cmp     %rbx, %rdi
        jb      .Lchunk

        cmp     $20, %rsi               /* more than 20 digits, even if leading zeros */
        ja      .Loverflow
        test    %r10, %r10
        jnz     .Loverflow
        mov     %r8, (%r9)
        mov     %esi, %eax              /* return n (at most 20 here) */
        pop     %rbx
        ret

.Lbad_char:
        mov     $-FP_BAD_CHAR, %eax
        pop     %rbx
        ret
.Loverflow:
        mov     $-FP_OVERFLOW, %eax
        pop     %rbx
        ret
.Lempty:
        mov     $-FP_EMPTY, %eax
        ret
        .size   fp_u64_asm, .-fp_u64_asm

        .section .note.GNU-stack,"",@progbits /* no executable stack */
