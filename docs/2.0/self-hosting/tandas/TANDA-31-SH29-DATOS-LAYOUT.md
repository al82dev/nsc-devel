# NativeSharp 2.0 self-hosting — tanda 31: datos, layout y globals

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH29` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `355fb76da` |
| Commit de cierre | este commit (`selfhost(tanda-31): lower static data and verify layouts`) |

## Implementación

La semántica existente de aggregates se elevó a contrato de proyecto mediante
`--dump-project-layout`: enums con tipo subyacente, structs/clases, offsets de campos, `pack`,
`size`, tamaño y alineación salen de la misma compilación multimódulo que llega al backend.

`LlvmBackend.DeclareData` recorre los símbolos semánticos y emite globals escalares con:

- nombre estable derivado de nombre cualificado + tipo;
- inicializador constante tipado (`int`, `bool`, flotante o cero);
- mutabilidad/constancia y alineación contractual;
- cargas y lvalues por símbolo, no por spelling incidental.

Se corrigió además `public const`: el parser conservaba `const` en `ModifiersText`, pero sólo ponía
`Flag` cuando aparecía antes de la accesibilidad. `SemanticAnalyzer` acepta ambas formas.

## Fallo real localizado y corregido

El primer objeto emitió `IMAGE_REL_AMD64_ADDR32` para los globals. LLD produjo un ejecutable que no
terminaba correctamente y NSLink lo rechazó explícitamente porque una dirección de imagen Win64 no
cabe en 32 bits. El target machine del backend usa ahora `LLVMRelocMode.PIC`; LLVM emite
`IMAGE_REL_AMD64_REL32` (RIP-relative). La misma pareja de globals enlaza y devuelve 42 con ambos
linkers.

La binding LLVM-C incorpora `LLVMGetNamedGlobal`, necesario para que definición, carga y store
consulten el mismo símbolo LLVM.

## Fixture y contrato

`selfhost/tests/data-layout/` fija:

- `Mode : byte`, tamaño/alineación 1/1;
- `Pair`, tamaño/alineación 8/4, offsets 0 y 4;
- `PackedPair`, tamaño/alineación 5/1;
- `Header`, layout 4/4;
- `GlobalAnswer = 40` y `const GlobalTwo = 2`;
- `Main` carga ambos globals y devuelve 42.

El gate `nsc.selfhost.data-layout` compila O0–O3, compara layout byte a byte, inspecciona símbolos y
relocations, enlaza por LLD y NSLink y ejecuta ocho binarios. Resultado: **PASSED**, 13.07 s.

## Archivos principales

- `selfhost/nsc/CodeGen/LlvmBackend.ns`: `LlvmGlobalName`, `ConstantInitializer`, `DeclareData`;
- `selfhost/nsc/Sema/SemanticAnalyzer.ns`: const global correcto;
- `lib/NativeSharp.LLVM.C/LLVMApi.ns`: `LLVMGetNamedGlobal`;
- `selfhost/nsc/Driver/Program.ns`: `DumpProjectLayout`;
- contrato, verificador Python y gate CMake SH29;
- fixture `selfhost/tests/data-layout/`.

- [x] enums/structs/clases y offsets medidos;
- [x] globals mutables y constantes con inicialización estática;
- [x] relocations Win64 válidas para LLD y NSLink;
- [x] matriz O0–O3 × dos linkers;
- [ ] baseline completo acumulado al final.
