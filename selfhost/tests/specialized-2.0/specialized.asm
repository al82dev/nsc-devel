.code
PUBLIC sh47_nsasm_probe
PUBLIC sh47_nsasm_instruction_surface

sh47_nsasm_probe PROC
    sub rsp, 24
    stmxcsr dword ptr [rsp]
    mov eax, 1
    movd xmm0, eax
    pshufd xmm0, xmm0, 0
    paddd xmm0, xmm0
    roundss xmm0, xmm0, 0
    pause
    rdtsc
    ldmxcsr dword ptr [rsp]
    add rsp, 24
    mov eax, 42
    ret
sh47_nsasm_probe ENDP

; This routine is inspected, not executed. It proves that NSAsm accepts the 2.0
; SIMD/FMA/packed surface without making the gate depend on optional host CPU features.
sh47_nsasm_instruction_surface PROC
    vaddps ymm0, ymm1, ymm2
    vfmadd213ps ymm0, ymm1, ymm2
    packssdw xmm0, xmm1
    vzeroupper
    ret
sh47_nsasm_instruction_surface ENDP
END
