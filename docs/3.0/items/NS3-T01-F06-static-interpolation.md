# NS3-T01-F06 — Static interpolation

Estado: terminado.

## Cambios

- Se creó una autoridad única `StaticInterpolationHandler` para recorrer partes en orden textual.
- Parser y binder dejaron de mantener clasificadores duplicados de holes constantes.
- El handler built-in no usa `NativeSharp.Core.String`, heap ni locale y termina como literal.
- Se corrigió el ancho decimal con signo: `-42:D5` ahora es `-0042`.
- Se corrigió `-0`, que ahora se renderiza como cero canónico.
- Se midió un handler de tipo usuario seleccionado en compile time.
- Se agregó una prueba O0 end-to-end para escapes, formatos, `noheap`, LLVM sin llamadas `String`,
  ejecución y fallos por handler ausente o token mal formado.

## Archivos

- `include/nativesharp/Language/ExpressionErgonomics.h`
- `src/Language/ExpressionErgonomics.cpp`
- `src/Parser/Parser.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `tests/unit/NativeSharp3T01ExpressionTests.cpp`
- `tests/unit/NativeSharp3T01StaticInterpolationTests.cpp` (nuevo)
- `CMakeLists.txt`
- `docs/3.0/language/static-interpolation.md` (nuevo, ES/EN)
- `docs/3.0/TANDA-OPERATIVA-06-T01-NULL-RANGE-INTERPOLATION-WITH.md`
- `docs/3.0/items/NS3-T01-F06-static-interpolation.md` (nuevo)

Los archivos nuevos separan la prueba ejecutable del helper sintáctico compartido y dejan un
contrato de usuario localizable sin depender del expediente histórico de una tanda anterior.

## Evidencia dirigida

- `nsc_ns3_t01_expression_tests`: `PASS: 31 checks`.
- `nsc_ns3_t01_static_interpolation_tests`: `PASS: 6 checks`.
- `nsc.exe`: construido; verificación de host runtime estático aprobada.
- Programas NativeSharp: sólo O0 y NSLink local.
- Baseline, CTest general, O1, O2, O3, instalación y deploy: no ejecutados.
