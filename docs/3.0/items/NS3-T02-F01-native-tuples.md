# NS3-T02-F01 — Tipos tupla nativos / Native tuple types

Estado: terminado.

## Cambios

- Se cerró la contradicción entre parser y semántica para las aridades 0 y 1.
- `()` y `(T,)` ahora son tipos estructurales completos; `(T)` continúa siendo agrupación.
- El predictor sintáctico reconoce esas formas en returns, parámetros, locals y genéricos.
- La clave canónica acepta y reconstruye aridades 0, 1, 2–16 y grandes sin colisiones.
- La tupla unidad usa un carrier estable de 1 byte/alineación 1; no se emiten valores zero-size.
- Nombres de elementos permanecen fuera de identidad y ABI.
- Se comprobaron carriers directos e indirectos con el ABI Windows x64 y NSLink.
- Move-only/`owned` no se presenta como cerrado: las operaciones que necesitan drops parciales
  conservan rechazo temprano hasta T06.

## Archivos

- `src/Parser/Parser.cpp`: gramática y lookahead de tuplas unidad/unarias.
- `src/Frontend/ProjectTypeConversion.cpp`: round-trip canónico para cualquier aridad.
- `src/Frontend/ProjectObjectLayoutModel.cpp`: carrier físico de la tupla unidad.
- `src/Frontend/ProjectMethodBodyModel.cpp`: literales de aridad 0 y 1.
- `tests/unit/NativeSharp3T02NativeTupleTests.cpp`: prueba O0 independiente (nuevo).
- `CMakeLists.txt`: registra el target dirigido.
- `docs/3.0/language/native-tuples.md`: contrato normativo ES/EN (nuevo).
- `docs/3.0/TANDA-OPERATIVA-07-T02-TUPLAS-DECONSTRUCCION.md`: actualiza el expediente histórico.
- `docs/3.0/items/NS3-T02-F01-native-tuples.md`: expediente del ítem (nuevo).

Los archivos nuevos separan el contrato de tipo/ABI de las pruebas combinadas de T02 y dejan una
referencia normativa localizable. No se creó un segundo modelo de layout ni otra ruta de lowering.

## Evidencia dirigida

- `nsc_ns3_t02_native_tuple_tests`: `PASS: 24 checks`.
- `nsc_ns3_t02_aggregate_tests`: `PASS: 13 checks`.
- `c57_ns3_tuples`, O0 + NSLink local: compiló/enlazó y terminó con código 42.
- `nsc.exe`: construido; contrato de host runtime estático aprobado.
- Baseline, CTest general, O1, O2, O3, instalación y deploy: no ejecutados.
