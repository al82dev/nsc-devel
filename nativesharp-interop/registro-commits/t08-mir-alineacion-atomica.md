# Evidencia de alineación atómica en MIR

T08-015: MIR conserva la alineación opcional calculada para un global importado
volatile. La alineación predeterminada de un byte no demuestra por sí misma que
Clang haya acreditado el acceso. El dato se copia desde HIR y se serializa; la
clave de caché cambia a v5 para evitar restaurar el formato anterior.

Este cambio no altera la emisión LLVM. La restauración se comprobará en la
validación agrupada de la tanda; aún no se afirma validación de este cambio.
