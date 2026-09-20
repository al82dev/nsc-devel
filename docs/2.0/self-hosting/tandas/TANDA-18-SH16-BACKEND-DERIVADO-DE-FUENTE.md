# NativeSharp 2.0 self-hosting — tanda 18: backend derivado de fuente

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Gate | `SH16` |
| Rama | `development` |
| Commit de entrada | `b8fe9f6fd` |
| Commit de cierre | este commit (`selfhost(tanda-18): derive backend objects from source`) |
| Fecha | 2026-09-20 |
| Baseline | `tanda-18-sh16-source-derived-backend-final-20260920` |

## 2. Objetivo, alcance y no objetivos

El objetivo es destruir la propiedad “todas las fuentes producen el mismo objeto”. `nsc-next`
ahora entrega al backend el árbol y el modelo semántico reales. La rebanada soporta un `Main` con
retorno entero compuesto por literales, `+`, `-`, `*`, `/`, más unarios `+`/`-`, y conserva O0–O3.

No incluye locales, parámetros, llamadas, memoria, clases, excepciones, múltiples archivos ni la
generación del propio compilador. SH16 no reabre todavía `--bootstrap-generation`.

## 3. Diseño y flujo de datos

`Program.EmitBackend` ejecuta Source → Lexer → Parser → Sema → IR verifier y conserva `SyntaxTree`
y `SemanticModel`. `LlvmBackend.FindEntryExpression` recorre funciones libres, namespaces y tipos
para seleccionar el retorno directo de un `Main` declarado como función o método.
`EmitI32Expression` recorre el AST recursivamente, obtiene el valor normalizado de literales desde
el hecho semántico y crea constantes o instrucciones LLVM-C. El valor producido alimenta
`NativeSharpCompute`; `mainCRTStartup` lo entrega a `ExitProcess`.

Invariantes: no existe cálculo fijo 6×7; una construcción no soportada falla sin objeto; los
objetos de 17 y 42 tienen hashes distintos; los ejecutables observan el valor de su propia fuente;
LLVM sólo cruza `NativeSharp.LLVM.C`; O0 y O3 conservan semántica.

## 4. Archivos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modificar | bajar AST entero a LLVM-C |
| `selfhost/nsc/Driver/Program.ns` | modificar | conservar y entregar frontend/semántica al backend |
| `selfhost/tests/codegen/source-derived-17.ns` | crear | fuente diferencial por resta |
| `selfhost/tests/codegen/source-derived-42.ns` | crear | fuente diferencial por multiplicación |
| `selfhost/contracts/source-derived-backend.contract.json` | crear | contrato SH16 |
| `selfhost/bootstrap/verify_source_derived_backend.py` | crear | hashes y ejecución de ocho combinaciones |
| `cmake/VerifyNativeSharpSourceDerivedBackend.cmake` | crear | construcción, emisión y enlace dual |
| `cmake/VerifyNativeSharpLlvmBackend.cmake` | modificar | sustituir fixture general por uno dentro del alcance real |
| `CMakeLists.txt` | modificar | registrar prueba acumulativa SH16 |
| documentación y prueba Python | modificar/crear | contrato humano y regresión estructural |

## 5. Tipos, métodos y funciones

No se añade tipo público. Cambia la firma a
`EmitExecutableObject(SyntaxTree, SemanticModel, string, BackendOptimization, BackendArtifactKind)`.

| Método | Responsabilidad | Salida/error/efectos |
|---|---|---|
| `FindEntryExpression(SyntaxTree, int): int` | localizar recursivamente el retorno directo de `Main` | id o -1; sólo lectura |
| `EntryExpression(SyntaxTree): int` | iniciar la búsqueda desde la raíz | id o -1; sólo lectura |
| `EmitI32Expression(..., int, out bool): void*` | bajar recursivamente expresión a LLVM i32 | valor LLVM; `valid=false` si queda fuera del subconjunto |
| `Program.EmitBackend(...)` | crear frontend validado y emitir artefacto | 0 o diagnóstico de backend; escribe sólo salida solicitada |
| `verify_source_derived_backend.main(): int` | verificar fuentes, hashes y ejecuciones | reporte SH16 o primer error |

El lowering usa `LLVMConstInt`, `LLVMBuildAdd`, `LLVMBuildSub`, `LLVMBuildMul` y `LLVMBuildSDiv`.
Los objetos LLVM siguen ownership de módulo/contexto y se liberan mediante `Dispose`; la operación
es single-thread y no conserva punteros del AST fuera de la llamada.

## 6. Superficie observable y compatibilidad

`--emit-object` y `--emit-assembly` mantienen su CLI, pero ahora rechazan una fuente sin un retorno
directo soportado (código interno 12, mostrado como error de backend). Las fuentes anteriores que
se usaban sólo para validar frontend continúan en sus pruebas propias. La prueba SH08 usa ahora la
fixture 42 que corresponde a la capacidad verdadera.

## 7. Pruebas y comparación diferencial

`test_tanda18_backend_is_parameterized_by_the_parsed_source` prohíbe el cálculo fijo. SH16 compila
17 y 42 en O0/O3, enlaza cada objeto con `lld-link` y NSLink y ejecuta ocho PEs. La diferencia
respecto de SH08 es intencional: el resultado ya no está fijado por el backend, sino por el AST. No
hay normalización de objetos; sus hashes deben diferir dentro de cada perfil.

Evidencia: `selfhost/evidence/gates/SH16/sh16-lld-report.json`,
`sh16-nslink-report.json` y `gate.json`.

## 8. Baseline de cierre

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work ^
  --temp-root A:\nsc-devel\selfhost\out\temp --write-root A:\ ^
  --run-id tanda-18-sh16-source-derived-backend-final-20260920 ^
  --build-id 2000000000000000
```

G00 y el reporte de corrida son autoritativos. La matriz esperada es 451/451 CTest por enlazador,
148/148 fixtures y 18/18 triage por enlazador. El baseline se ejecuta después del último cambio.

La corrida anterior `tanda-18-sh16-source-derived-backend-20260920` falló 2/451 en ambos árboles:
SH09 y SH10 usan `Main` como método dentro de una clase, mientras la primera implementación sólo
buscaba funciones libres. La búsqueda recursiva corrigió la causa; no se añadió fallback constante.

## 9. Riesgos, deuda e higiene

El subconjunto es deliberadamente estrecho y una división entre cero todavía queda a LLVM/runtime.
La siguiente rebanada debe introducir funciones, parámetros y llamadas antes de abordar clases y
runtime. `nsc0` sigue predeterminado. El commit no incluye binarios, `out`, temporales ni los cambios
locales ajenos ya presentes en el árbol.

## 10. Criterio de cierre

- [x] Dos fuentes producen objetos distintos y resultados distintos.
- [x] O0/O3 y ambos linkers aprueban.
- [x] La regresión fija la dependencia AST/semántica.
- [x] Baseline y evidencia SH16 quedan ligados a este commit.
