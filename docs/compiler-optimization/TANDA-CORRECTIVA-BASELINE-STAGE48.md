# Tanda correctiva — gates administrados Stage 48

**Fecha:** 1 de octubre de 2026  
**Rama:** `development`  
**Plataforma:** Windows x64  
**Validación:** dirigida; baseline no ejecutado

## Problemas observados

El baseline de NativeSharp 3.0 produjo dos falsos negativos en la integración administrada:

- Stage 48.5 compiló correctamente el host C#, pero el test buscó el ensamblado en
  `bin/Release/net9.0` aunque `<PlatformTarget>x64</PlatformTarget>` lo publica en
  `bin/x64/Release/net9.0`.
- Stage 48.6 ejecutó su gate sin indicar qué compilador debía probar. El script eligió
  `C:\native-sharp-3.0\nsc.exe`, mezclando un producto instalado con la configuración que el
  baseline acababa de construir en A:.

Además, Stage 48.6 borraba siempre el directorio temporal, incluso al fallar, por lo que también
destruía `cppcli_windows_gate.log`.

## Cambios

- `tests/unit/Stage48_5CSharpPInvokeHostTests.cpp`: comprueba la ruta x64 real del ensamblado.
- `CMakeLists.txt`: entrega a Stage 48.6 la ruta de `$<TARGET_FILE:nsc>` de la configuración activa.
- `tests/unit/Stage48_6CppCliHostClosureTests.cpp`: pasa explícitamente ese compilador al gate y
  conserva evidencia cuando falla mediante `evidenceRoot`/`resultKeeping`.
- `examples/project_stage48_6/build_and_run_windows_cppcli_host.cmd`: elimina el fallback a C:;
  el gate exige ahora una ruta explícita a `nsc.exe`.

## Mejora

El baseline ya no puede certificar accidentalmente otro compilador. Stage 48.5 distingue un build
correcto de un error de ruta, y Stage 48.6 deja diagnóstico reproducible si MSBuild, .NET o el host
final fallan.

## Validación prevista

- construir `nsc_stage48_5_csharp_host_tests` y `nsc_stage48_6_cppcli_host_tests` en la configuración
  O0 existente;
- ejecutar Stage 48.5 individualmente;
- no ejecutar el gate completo Stage 48.6 durante esta tanda porque su fixture NativeSharp está
  fijada a O2; su ejecución completa queda para el baseline del usuario.
