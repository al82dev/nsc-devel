# NativeSharp 2.0 self-hosting — tanda 20: locales y control de flujo

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH18` |
| Rama | `development` |
| Commit de entrada | `368c6af45` |
| Commit de cierre | este commit (`selfhost(tanda-20): lower locals and control flow`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-20-sh18-source-control-flow-20260920` |

## 2. Objetivo, alcance y no objetivos

SH18 convierte cuerpos lineales en CFG fuente: declara locales `int`, carga y almacena por símbolo,
baja asignación `=`, comparaciones enteras, `if` y `while`. La fixture suma 1..6, verifica 21 y
devuelve 42. No incluye `for`, `do`, break/continue, booleanos almacenados, parámetros asignables,
scopes con destrucción, objetos, arrays, strings, excepciones ni proyecto completo del compilador.

## 3. Diseño e invariantes

`FunctionLoweringState` posee una lista de `LocalBinding`; cada binding retiene sólo el id semántico
y un handle LLVM no propietario. `EmitStatement` baja bloques en orden y propaga si la ruta quedó
terminada. Los `if` crean then/else/end y los `while` condition/body/end. `EmitCondition` separa el
valor i1 de comparación del valor i32 general. Cada bloque emitido recibe terminador y cualquier
construcción fuera del alcance falla cerrada, sin constante ni objeto sustituto.

Decisiones: slots identificados por símbolo y no por nombre; comparación signed para `int`; alloca
en el punto de declaración durante esta rebanada; dos fases de funciones de SH17 se conservan.

## 4. Archivos, tipos y métodos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | estado local, expresiones con memoria y CFG |
| `selfhost/tests/codegen/source-control-flow.ns` | crear | suma y decisión observable |
| `selfhost/contracts/source-control-flow.contract.json` | crear | contrato SH18 |
| `selfhost/bootstrap/verify_source_control_flow.py` | crear | símbolos, hashes y ocho ejecuciones |
| `cmake/VerifyNativeSharpSourceControlFlow.cmake` | crear | construir, inspeccionar y enlazar matriz |
| `CMakeLists.txt` y prueba Python | modificar | gate acumulativo y regresión estructural |
| roadmaps/readmes/expediente | modificar/crear | frontera humana auditable |

`LocalBinding(int,void*)` expone `Symbol()` y `Storage()`; el objeto es propiedad de la lista y el
handle es propiedad del módulo LLVM. `FunctionLoweringState.AddLocal` transfiere el binding a la
lista y `FindLocal` busca desde el último scope. `EmitCondition(...): void*` produce i1 o marca
inválido. `EmitStatement(...,out bool): bool` distingue éxito de terminación. `Comparison` y
`ComparisonPredicate` centralizan la tabla de operadores. `EmitI32Expression` carga locales y
almacena asignaciones simples. La operación es single-thread y todo estado muere con la función.

## 5. Superficie, pruebas y divergencias

La CLI no cambia. El contrato schema `nativesharp-selfhost-source-control-flow` v1 y el reporte
`nativesharp-selfhost-sh18-report` declaran scope `i32-locals-assignment-if-while`. Se usan
`LLVMBuildAlloca`, `LLVMBuildLoad2`, `LLVMBuildStore`, `LLVMBuildICmp`, `LLVMBuildBr` y
`LLVMBuildCondBr` ya congelados por la frontera LLVM-C.

Las 38 pruebas unitarias pasan. La primera prueba dirigida falló sólo durante inspección porque el
CTest nuevo recibió por error una variable CMake inexistente como linker; el objeto era COFF válido
y ejecutaba 42. Se corrigió a `${CMAKE_LINKER}` y ambos árboles pasaron SH18 en 13 s. No se cambió
el verificador ni se relajó la anatomía.

## 6. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-20-sh18-source-control-flow-20260920 ^
  --build-id 2000000000000000
```

Resultado: `PASSED` en 856.16 s. La matriz cerró 453/453 CTest, 148/148 fixtures y 18/18 triage
por enlazador; no hubo problemas. G00 quedó `LANDED`, con digest de fuentes
`6b513ed9c6b12e31ad424b8ec775b973cfa4730b1bcb28f41e5cb5cb7dfb5458`. Los reportes SH18
registran ocho ejecuciones correctas —O0, O1, O2 y O3 con `lld-link` y NSLink— y objetos COFF
derivados del fuente. Los hashes completos viven en `selfhost/evidence/gates/SH18/gate.json`.

## 7. Riesgos, higiene y cierre

La asignación compuesta, shadowing con salida de scope, alloca canónica en entry y control de flujo
completo quedan como deuda. SH18 no autoriza self-hosting completo ni promoción. El commit excluye
`out`, binarios, temporales y los cambios ajenos en `docs/1.0.1`, `mkdown` y `Claude outputs`.

- [x] Fixture source-derived devuelve 42 en la matriz dirigida.
- [x] SH16 y SH17 permanecen verdes.
- [x] Baseline completo 453/453 y evidencia SH18 versionada.
