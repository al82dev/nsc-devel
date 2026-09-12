# Entrega 233 — Fronteras MIR y Runtime ABI / MIR and Runtime ABI boundaries

## Objetivo / Objective

**ES.** Convertir dos contratos internos implícitos —la forma de MIR consumida por herramientas y
el conjunto de símbolos del runtime nativo— en identidades explícitas, versionadas y comprobables.
La entrega no cambia nombres físicos, firmas ni semántica: congela el estado compatible actual.

**EN.** Turn two implicit internal contracts—the MIR shape consumed by tools and the native runtime
symbol set—into explicit, versioned, verifiable identities. This delivery changes no physical
names, signatures, or semantics; it freezes the current compatible state.

## Archivos creados / Created files

### `include/nativesharp/Runtime/RuntimeAbiContract.h`

Declara `kRuntimeSymbolSetVersion`, `RuntimeAbiSymbolKind`, `RuntimeAbiSymbol` y el inventario
canónico ordenado `kRuntimeAbiSymbols`. El inventario incluye funciones y datos propios
materializados por `WindowsRuntimeBuilder`; separa la identidad del conjunto de la versión de
firmas y comportamiento `runtime_abi=1.0`.

Declares `kRuntimeSymbolSetVersion`, `RuntimeAbiSymbolKind`, `RuntimeAbiSymbol`, and the ordered
canonical `kRuntimeAbiSymbols` inventory. It includes NativeSharp functions and data materialized by
`WindowsRuntimeBuilder` and separates symbol-set identity from the `runtime_abi=1.0` signature and
behavior contract.

## Archivos modificados / Modified files

- `include/nativesharp/Frontend/ProjectOopMirModel.h`: define la versión `1.0` del esquema MIR.
- `src/Frontend/ProjectOopMirModelDumper.cpp`: publica `schema=1.0` en cada dump MIR.
- `include/nativesharp/Driver/ToolchainContract.h`: eleva el contenedor a `NSTOOLCHAIN 2`.
- `src/Driver/ToolchainContract.cpp`: agrega `runtime_symbol_set=1` y `mir_schema=1.0` al contrato
  canónico y, por la verificación existente, los vuelve campos obligatorios y ordenados.
- `tests/unit/WindowsRuntimeTests.cpp`: recorre el inventario completo y exige que cada símbolo se
  materialice en el módulo LLVM del runtime.
- `tests/unit/Stage62ToolchainContractTests.cpp`: cubre formato 2, ambos campos, orden, cardinalidad y
  diagnóstico por ausencia de la versión MIR.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`: agrega `NSC-BOUNDARY-001` y actualiza
  el contrato de distribución.
- `docs/1.0.1/compiler/NativeSharp_Compiler_1_0_1_conformance.map`: enlaza regla, implementación y
  evidencia.

## Implementación y decisiones / Implementation and decisions

1. **Versiones independientes.** El esquema MIR, la ABI del runtime y el conjunto de símbolos
   evolucionan por motivos distintos. Un nodo MIR nuevo no rompe por sí mismo un binario; un helper
   agregado no necesariamente cambia una firma existente. Mantener tres identidades evita subir
   una versión ajena o esconder una ruptura real.
2. **Inventario compilado, no documento suelto.** La lista se expresa como datos C++ `constexpr` y
   la prueba construye el runtime real. Así, una edición accidental del builder que omita un helper
   se vuelve una falla ejecutable y no una divergencia documental silenciosa.
3. **Formato de toolchain elevado.** Dos campos normativos nuevos cambian el esquema estricto del
   artefacto. `NSTOOLCHAIN 2` hace que herramientas antiguas rechacen el formato con claridad en vez
   de interpretar parcialmente un contrato 1.
4. **Dump compatible para lectores humanos.** `functions=N` conserva su posición y la versión se
   agrega en la misma cabecera. Las búsquedas existentes siguen siendo legibles, mientras un lector
   automático puede exigir `schema=1.0`.
5. **Sin renombrado ABI.** La entrega no sustituye literales dentro del builder ni cambia lowering.
   El inventario congela lo que ya se emite; una centralización física posterior deberá conservar
   exactamente estos nombres o incrementar el contrato correspondiente.

Independent versioning prevents unrelated changes from being mislabeled. The symbol list is
compiled and checked against the real runtime module, the strict toolchain container advances to
format 2, and MIR dumps carry their own schema identity. Existing ABI spellings and lowering remain
unchanged.

## Verificación / Verification

Según la disciplina acumulativa, esta entrega añade evidencia dirigida pero no la ejecuta todavía.
Después del commit se reconstruye desde cero únicamente el objetivo `nsc`; los tests y el baseline
se ejecutan una sola vez después de la entrega 234.

Under the cumulative discipline, this delivery adds targeted evidence without running it yet. Only
a clean `nsc` rebuild follows the commit; tests and the baseline run once after delivery 234.

## Mensaje de commit / Commit message

### Título ES / Spanish title

`Congela las fronteras MIR y Runtime ABI`

### Title EN / English title

`Freeze the MIR and Runtime ABI boundaries`

### Descripción ES / Spanish description

`Versiona el esquema MIR y el conjunto de símbolos del runtime, publica ambas identidades en NSTOOLCHAIN 2 y agrega comprobaciones ejecutables que impiden divergencias silenciosas entre contrato, dumps y módulo nativo.`

### Description EN / English description

`Version the MIR schema and runtime symbol set, publish both identities in NSTOOLCHAIN 2, and add executable checks that prevent silent drift between the contract, dumps, and native module.`
