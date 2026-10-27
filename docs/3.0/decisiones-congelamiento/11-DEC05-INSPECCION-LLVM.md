# Congelamiento 3.0 DEC-05 — CFG, dominancia, SSA y optimización sobre el LLVM del pipeline 3.x

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `8c1559649`.

## Propósito y problema

C-05 (`2e154f127`) hizo que un `.ns` compile como su proyecto implícito y borró `DriverLegacyFrontend.cpp`. Desde
entonces `--dump-cfg`, `--verify-cfg`, `--dump-dominance`, `--verify-dominance`, `--dump-ssa`, `--verify-ssa`,
`--dump-opt`, `--verify-opt` y `--dump-pass-pipeline` salen con código 2 (hallazgo A-04).

Decisiones de Alberto Huerta Luna (2026-10-26):

- DEC-05: no restaurar el pipeline 1.x; restaurar esas acciones públicas sobre el pipeline 3.x cuando sean
  aplicables.
- DEC-15: C-05 queda autorizado como pipeline 3.x unificado.
- Respuesta a la pregunta de diseño: las nueve acciones observan el LLVM del proyecto implícito.
  - No se resucita el MIR 1.x ni se construye un adaptador hacia `CfgBuilder`, `Dominance`, `SSA` y `Passes`.
  - El formato nuevo queda versionado y documentado.
  - `--verify-ssa` valida las invariantes SSA de LLVM sin suponer que desaparecen todos los `alloca`.
  - Los dumps son deterministas y no modifican el módulo usado para generar código.

## SCOPE LOCK

- **Objetivo.** Las nueve acciones vuelven a funcionar (exit 0) sobre el LLVM del pipeline 3.x, con formatos
  `nsc-llvm-*/1`.
- **Naturaleza.** Restauración de capacidad pública por decisión explícita.
- **Debe quedar idéntico.** Las demás acciones de archivo y de proyecto, la generación de código y las librerías
  `src/CFG`, `src/Dominance`, `src/SSA` y `src/Passes` (siguen sin consumidor en el driver).
- **Modificable.** `DriverImplicitProject.cpp`, `DriverHelp.cpp`, `LlvmModule.h` (amistad), `LlvmCApi.h`
  (declaraciones), `LlvmModuleInspection.h/.cpp` (nuevos), `CMakeLists.txt` (fuente y propiedades CTest de las
  nueve pruebas), los nueve `tests/cases` de esas acciones y los nueve bloques de `tests/unit/DriverTests.cpp`.
- **Prohibido.** Correr el pipeline 1.x; cambiar el módulo que genera código.
- **Terminación.** Las nueve acciones sobre los fuentes de `tests/integration/valid` salen con 0 y cumplen las
  expectativas restauradas.

## Cambio

- `llvm_backend::LlvmModuleInspector` (`include/nativesharp/LLVM/LlvmModuleInspection.h`,
  `src/LLVM/LlvmModuleInspection.cpp`).
  - Construye el grafo de cada función definida con la API C de LLVM.
  - Calcula el postorden inverso, los dominadores inmediatos (Cooper, Harvey y Kennedy) y las fronteras, sin
    recursión.
  - Vuelca y verifica CFG, dominancia y SSA. La forma SSA es `mem2reg` sobre `LLVMCloneModule`.
- `LlvmModule` declara amiga a `LlvmModuleInspector`. `LlvmCApi.h` declara las funciones de la API C que usa
  (vuelven `LLVMGetFirstBasicBlock` y `LLVMGetNextBasicBlock`).
- `Driver::runSourceFile`:
  - CFG, dominancia y SSA bajan el LLVM sin optimizar del proyecto implícito con `lowerSourceFileModule` y lo
    inspeccionan.
  - `--dump-opt` y `--verify-opt` son `--dump-project-oop-llvm-opt` y `--verify-project-oop-llvm-opt` con
    encabezado versionado.
  - `--dump-pass-pipeline` compila el archivo e imprime el pipeline LLVM del nivel.
- Ayuda: vuelven las nueve líneas de uso, con una línea en inglés y español sobre el módulo observado.
- Especificación de los formatos: `docs/3.0/NSC-FORMATOS-INSPECCION-LLVM.md`.

## Pruebas

C-05 había cambiado las expectativas de estas nueve acciones a «rechazo con código 2». Con DEC-05 esa
expectativa es falsa. Criterio DEC-16: volver byte por byte a lo anterior a C-05 donde la expectativa histórica se
cumple, y cambiar solo lo que el formato nuevo no puede cumplir.

- `tests/cases` restaurados byte por byte a `2e154f127~1`: control-flow (dump-cfg, verify-cfg), cfg-features
  verify, dominance (dump, verify), ssa verify y los tres de pass-features.
- `tests/cases/flow/cfg-features/dump.case.json`: `[unreachable]` pasa a una regex del bloque `while.cond` de
  `Select` con preds `[bb0, bb<n>]`.
- `tests/cases/flow/ssa/dump.case.json`: `phi @value` pasa a la regex `ssa_features[$]3A[$]3A__Main phis=[1-9]`.
- CTest `nsc.dump-cfg` … `nsc.dump-pass-pipeline`: expresiones previas a C-05, con las mismas dos excepciones.
- `DriverTests.cpp`: los nueve bloques restaurados a `2e154f127~1`; en `--dump-pass-pipeline`,
  `dead-code-elimination` pasa a `pass-pipeline: default<O0>`.

Las tres excepciones nombran cosas del pipeline 1.x:

- el bloque muerto después de `return`: el lowering 3.x no lo emite, y LLVM no tiene bloques inalcanzables en
  `cfg_features.ns`;
- el phi del símbolo `value`: en LLVM se llama `%local0.addr.0`;
- el pase MIR `dead-code-elimination`.

Los `.ns` que C-05 portó al lenguaje 3.x no cambian.

## Validación

nsc Release recompilado (exit 0). Sonda `A:\temp\nsc-dec\d05\probe.cmd`:

- Las nueve acciones sobre `control_flow`, `cfg_features`, `dominance_features`, `ssa_features` y
  `pass_features`: exit 0, stderr vacío.
- Cumplen las expresiones CTest. Se comprobaron con `cmake -P` sobre las salidas, y las de `tests/cases` con el
  `re.search(..., re.DOTALL)` del arnés.
- `--verify-cfg`, `--verify-dominance` y `--verify-ssa` sobre `f010`, `f064`, `f089`, `k1` y `a3` del corpus de
  await (corrutinas, generadores, EH): exit 0. Por ejemplo, `a3`: 820 funciones, 6584 bloques, 579 phis y 189
  allocas no promovibles.
- `--dump-pass-pipeline -O2`: `pass-pipeline: default<O2>`.
- El fuente de `DriverTests` (`int Main() { long x = 42; return (int)x; }`) cumple las expectativas restauradas de
  los nueve bloques.

## Hallazgo (no corregido aquí)

`f078` (generador async con `raii Box` y `panic`) compila como ejecutable, pero como biblioteca falla con
`NativeSharp 3.0 value drop has no address/member model`. Pasa igual con `--verify-llvm`, que ya existía, y por lo
tanto también con las acciones nuevas, que usan el proyecto implícito de biblioteca como `--dump-llvm`. Es un
rechazo explícito con mensaje interno, previo a este cambio. Queda para la verificación de colecciones y
destructores.

## No verificado

- `DriverTests` (unitarias), CTest y `tests/cases` completos: cierre final.
