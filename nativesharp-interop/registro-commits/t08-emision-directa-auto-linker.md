# Tanda 8: selección automática en emisión directa de ejecutables

`--emit-project-oop-exe` no pasaba por la selección automática de `nsc build`. Un proyecto C con TLS, CRT y ninguna elección explícita falló en NSLink con NSL1023. La emisión directa inspecciona ahora las referencias estáticas del grafo antes de calcular la identidad de caché y selecciona LLD cuando la inspección indica que es necesario. Si existe selección interna o `NSC_LINKER`, se conserva. Las bibliotecas no inspeccionables se tratan conservadoramente.

La construcción incremental y el validador dirigido pasaron: TLS O0–O3, bitcode, COFF sin TLS, prioridad explícita y caché incremental. También pasaron los controles existentes de `nsc build` y cambio de enlazador. El cambio no afecta la emisión directa de DLL, no modifica NSLink y no resuelve los cuatro controles de DLL que siguen abiertos.
