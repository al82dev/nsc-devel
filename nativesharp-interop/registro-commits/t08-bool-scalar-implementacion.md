# T08: global C escalar `volatile _Bool`

Para el caso reproducido se transporta al descriptor MIR la condición de
almacenamiento C volatile no `_Atomic`, bool de un byte y modo MS. La
declaración LLVM usa i8; HIR conserva la alineación y LLVM emite carga de byte
volatile acquire con normalización y escritura de byte volatile release.
La condición se restringe a global C y no cambia el bool nativo ni `_Atomic`.

La implementación está preparada sin construir ni validar. Faltan controles
mixtos e IR y la validación agrupada; no modifica NSLink.
