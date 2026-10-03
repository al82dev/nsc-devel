# NS3-T01-F07 — `with` expressions

Estado: terminado.

## Cambios

- Se auditó parser, binder, HIR, MIR y LLVM del vertical `with`.
- Se cerró una fuga de encapsulación: un update ya no puede nombrar campos privados desde otro
  tipo; dentro del tipo declarante continúa siendo válido.
- Se midieron fuente evaluada una vez, updates en orden textual y fuente sin mutación.
- Se cubrieron POD, generic structs, readonly, Clone de usuario, private-in-scope y `noheap`.
- Se conservaron rechazos tempranos para duplicados, clases y valores move-only/owner-bearing.
- Se comprobó el lowering `aggregate-spread` sin wrapper de layout ni heap implícito.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`: aplica accesibilidad real a cada campo nombrado.
- `tests/unit/NativeSharp3T01WithExpressionTests.cpp`: prueba O0 independiente (nuevo).
- `CMakeLists.txt`: registra el target dirigido.
- `docs/3.0/language/with-expressions.md`: contrato y equivalencia ES/EN (nuevo).
- `docs/3.0/TANDA-OPERATIVA-06-T01-NULL-RANGE-INTERPOLATION-WITH.md`: corrige el expediente
  histórico.
- `docs/3.0/items/NS3-T01-F07-with-expressions.md`: expediente del ítem (nuevo).

Los archivos nuevos aíslan la matriz de `with` de la fixture combinada c56 y vuelven localizable la
semántica normativa. No se creó una segunda ruta de lowering.

## Evidencia dirigida

- `nsc_ns3_t01_with_expression_tests`: `PASS: 7 checks`.
- `nsc.exe`: construido; contrato de host runtime estático aprobado.
- NativeSharp: exclusivamente O0 y NSLink local.
- Baseline, CTest general, O1, O2, O3, instalación y deploy: no ejecutados.
