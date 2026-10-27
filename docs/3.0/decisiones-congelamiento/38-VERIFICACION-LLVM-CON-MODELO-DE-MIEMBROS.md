# Congelamiento 3.0, elemento 114 — volcado y verificación LLVM de structs con destructor

Fecha: 2026-10-26. Revisión de partida: `ac5403f0e`.

## Nota de trabajo

```text
HEAD: ac5403f0e
Elemento: 114 del plan de 142 (defecto: f078 en modo biblioteca)
Contrato vigente: C-05 / DEC-05 (las acciones de inspección observan el LLVM 3.x) y C-11 (structs con destructor)
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  src/Driver/DriverImplicitProject.cpp
  src/Driver/DriverProjectActions.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

`nsc --verify-llvm F078Gen.ns` terminaba con `nsc: error: NativeSharp 3.0 value drop has no address/member
model`. Como ejecutable, el mismo programa corre.

La causa no es el generador. Sondas `A:\temp\nsc-dec\d21`:

| Sonda | Forma | `--verify-llvm` antes |
|---|---|---|
| V1Plain, V2Raii, V3Panic, V5NoGuard | generador async con y sin `raii`/`panic`, sin struct con destructor | ok |
| V7SyncGen | generador síncrono con `raii` | ok |
| V6GuardOnly | sólo declara `struct Guard { ~Guard() {...} }` y `Main` devuelve 0 | el error |
| F078Gen | generador + el mismo `Guard` sin usar | el error |

Un struct con destructor basta. Las acciones de volcado y verificación bajaban el módulo con
`ProjectOopLlvmLoweringRequest::withoutDebug`, que no lleva el modelo de miembros. Los drops de valor de C-11
(`emitValueDropAddress`, el drop glue de cada struct) lo necesitan y fallan sin él; otros sitios de C-11 que
preguntan `memberModel_ != nullptr` omitían el drop en silencio, así que el LLVM inspeccionado tampoco era el
que se emite. La emisión del ejecutable usa `withDebug` con el modelo.

## SCOPE LOCK

- **Cambia.** Las dos rutas de inspección (`lowerSourceFileModule` del proyecto implícito de un `.ns` y
  `lowerProjectLlvm` de las acciones `--dump-project-oop-llvm*` y `--verify-project-oop-llvm*`) bajan con
  `withDebug(..., DebugInformationKind::None, LlvmOptimizationLevel::O0)`: el modelo de miembros y el
  `SourceManager`, sin información de depuración y con el mismo nivel O0 que daba `withoutDebug`.
- **Idéntico.** El backend, la emisión de ejecutables, bibliotecas y DLL, y `withoutDebug` (lo siguen usando
  `ProjectOopLlvmLowerer::lower(nameLookup, mirModel)` y sus pruebas).
- **Consecuencia aceptada.** Lo que se vuelca o se verifica ahora incluye los drops de valor que antes se omitían.
  Es el LLVM que se emite.

## Validación

nsc Release de este árbol.

| Prueba | Antes | Ahora |
|---|---|---|
| `--verify-llvm` de F078Gen y V6GuardOnly | el error | ok |
| `--verify-llvm` de V1–V3, V5 y V7 | ok | ok |
| `--verify-llvm` de A4FutureField | no medido | ok |
| volcado y verificación `-opt` de `d12\L5SpanReverseStruct` | no medido | ok; 5 `__nsc_drop_value` |
| lo mismo para `d23\F114ValueDropVerify` (fuente de la prueba nueva) | no medido | ok; 3 `__nsc_drop_value` |
| manifiestos de `tests/cases` con acción LLVM (`d23\llvmcases.py`, 23 casos) | — | 23 PASS |
| `focal.cmd f114` (casos de componente 96 y 30, `awcheck`) | — | sin cambios frente a `f113b` |

`llvmcases.py` corre cada manifiesto de `tests/cases` cuyos argumentos nombran una acción LLVM, con las reglas
del harness de `p7\casrev.py`; resultados en `d23\llvmcases-f114.txt`. No tengo la corrida de esos 23 casos con
el compilador anterior; todos pasan con el actual.

Prueba nueva: `Freeze30ContractTests::testValueDropVerification` (las dos acciones de proyecto sobre un struct con
destructor: éxito, `__nsc_drop_value_` en el volcado y `LLVM optimization verification: ok`). Compila en
`A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## No verificado

- Unitarias y suites que comparan texto de volcados LLVM (por ejemplo `DriverTests`): gate final.
