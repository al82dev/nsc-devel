# NS3-T02-F02 — Literales y conversión de tuplas / Tuple literals and conversions

Estado: terminado.

## Cambios

- Se auditó inferencia, binding de contexto, overload resolution, HIR y conversión LLVM.
- El clasificador semántico convierte recursivamente referencias de clase/interfaz dentro de
  tuplas; ya no cae por error al clasificador primitivo.
- Conversiones definidas por usuario dentro de tuplas quedan rechazadas hasta tener lowering de
  llamadas por elemento; no se reinterpretan carriers.
- El desempate de overloads usa el orden parcial de destinos cuando los rangos empatan.
- Inicialización y `return` informan el índice del primer elemento incompatible o ambos conteos de
  aridad.
- Se midieron inferencia natural, widening numérico, upcast de referencias, temporales, overloads,
  `return` contextual y evaluación única de izquierda a derecha.

## Archivos

- `src/Frontend/ProjectConversionClassifier.cpp`: conversión semántica recursiva por elemento.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: helper causal y contrato de desempate.
- `src/Frontend/ProjectMethodBodyModel.cpp`: diagnósticos por elemento y mejor destino de overload.
- `tests/unit/NativeSharp3T02TupleLiteralTests.cpp`: prueba O0 independiente (nuevo).
- `CMakeLists.txt`: registra el target dirigido.
- `docs/3.0/language/tuple-literals-and-conversions.md`: contrato ES/EN (nuevo).
- `docs/3.0/TANDA-OPERATIVA-07-T02-TUPLAS-DECONSTRUCCION.md`: actualiza evidencia histórica.
- `docs/3.0/items/NS3-T02-F02-tuple-literals.md`: expediente del ítem (nuevo).

La prueba nueva separa conversiones y overloads del vertical de deconstrucción. No se añadió una
ruta paralela de layout ni se debilitó el sistema de tipos para aceptar carriers del mismo tamaño.

## Evidencia dirigida

- `nsc_ns3_t02_tuple_literal_tests`: `PASS: 4 checks`.
- `nsc_project_type_conversion_tests`: `PASS: 22 checks`.
- `nsc_project_method_body_model_tests`: `PASS: 58 checks`.
- `nsc_ns3_t02_native_tuple_tests`: `PASS: 24 checks`.
- `nsc_ns3_t02_aggregate_tests`: `PASS: 13 checks`.
- `c57_ns3_tuples`, O0 + NSLink local: compiló/enlazó y terminó con código 42.
- `nsc.exe`: construido; contrato de host runtime estático aprobado.
- Baseline, CTest general, O1, O2, O3, instalación y deploy: no ejecutados.
