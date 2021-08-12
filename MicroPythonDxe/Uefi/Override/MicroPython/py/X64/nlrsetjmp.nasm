;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
;*   This program and the accompanying materials
;*   are licensed and made available under the terms and conditions of the BSD License
;*   which accompanies this distribution.  The full text of the license may be found at
;*   http://opensource.org/licenses/bsd-license.php
;*
;*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
;*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;*
;*    nlrsetjmp.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

extern ASM_PFX(nlr_push_tail)

global ASM_PFX(nlr_push)
ASM_PFX(nlr_push):
    pop     rdx                 ; load rip as well as fix rsp
    mov     [rcx + 0x10], rbx
    mov     [rcx + 0x18], rsp
    mov     [rcx + 0x20], rbp
    mov     [rcx + 0x28], rdi
    mov     [rcx + 0x30], rsi
    mov     [rcx + 0x38], r12
    mov     [rcx + 0x40], r13
    mov     [rcx + 0x48], r14
    mov     [rcx + 0x50], r15
    mov     [rcx + 0x58], rdx   ; rip

    push   rdx                      ;nlr_push_tail needs the return address
    jmp    ASM_PFX(nlr_push_tail)   ;do the rest in C

global ASM_PFX(asm_nlr_jump)
ASM_PFX(asm_nlr_jump):
    mov     rbx, [rcx + 0x10]
    mov     rsp, [rcx + 0x18]
    mov     rbp, [rcx + 0x20]
    mov     rdi, [rcx + 0x28]
    mov     rsi, [rcx + 0x30]
    mov     r12, [rcx + 0x38]
    mov     r13, [rcx + 0x40]
    mov     r14, [rcx + 0x48]
    mov     r15, [rcx + 0x50]

    mov     rax, rdx               ; set return value
    jmp     qword [rcx + 0x58]     ; rip

