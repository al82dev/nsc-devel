# Declaración de `LLVMSetThreadLocal`

La construcción falló porque la interfaz C de LLVM mantenida por NSC no
declaraba `LLVMSetThreadLocal`. Se añadió la firma `void(LLVMValueRef,
LLVMBool)` verificada contra `llvm-c/Core.h` de LLVM 23.1.0. No se modifica
LLVM externo. Pendiente de construcción incremental y validación TLS.
