# Tanda correctiva G00: identidad coherente de NativeSharp 3.0

## Problema

La rama y el proyecto CMake ya identificaban al compilador como `3.0.0`, pero la identidad
publicada del lenguaje continuaba en `2.0.0`. La contradiccion aparecia en
`--print-compiler-identity`, el servicio de lenguaje, el contrato del toolchain y varias pruebas.
Ademas, `nsc.version` exigia un banner historico que ya no corresponde al formato real de consola.

El cambio de identidad no debe activar automaticamente la edicion 3 para proyectos antiguos:
la ausencia de `<Language>` sigue resolviendose como `LanguageEdition::V2`.

## Cambios

- `include/nativesharp/Basic/Version.h`: publica lenguaje y compilador `3.0.0`, documentando
  por separado la compatibilidad de la edicion predeterminada V2.
- `CMakeLists.txt`: `nsc.version` valida el banner real y deriva la version desde `project()`,
  en vez de congelar texto 2.0 y un formato SemVer que el banner no imprime.
- `src/Driver/Driver.cpp`: la ayuda deja de presentar este ejecutable como la version estable
  2.0 y lo identifica correctamente como linea de desarrollo 3.0.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: los objetos generados registran productor 3.0.
- Las pruebas de identidad, driver, servicio de lenguaje, metadata, C++/CLI, ownership,
  toolchain y politica de compatibilidad esperan la identidad 3.0 actual.
- Las pruebas de contratos derivan Native ABI, Runtime ABI, runtime symbol set y MIR schema
  desde sus constantes canonicas. Esto elimina expectativas obsoletas `runtime_abi=1.0`,
  `runtime_symbol_set=2` y `mir_schema=1.14` sin debilitar la verificacion de presencia.

No se modifico `C:\native-sharp`, no se promovio 3.0 y no se cambio la edicion predeterminada
de los proyectos existentes.

## Mejora

Todas las superficies activas consultadas en esta tanda publican ahora una identidad coherente:

```text
compiler_version=3.0.0+build.<timestamp>.<hash>
compiler_release=3.0.0
language=3.0.0
```

El contrato generado conserva simultaneamente los valores fisicos vigentes de ABI y MIR, y
la prueba de banner ya comprueba la salida que realmente ofrece el compilador.

## Validacion especifica

- Construccion exclusiva del target `nsc` en `A:\nsc-devel\build-clang`.
- Identidad directa del ejecutable: compiler, release y language `3.0.0`.
- Identidad CC1: 9/9.
- Comando de identidad: aprobado.
- Driver: 230/230.
- Servicio de lenguaje: aprobado; la edicion predeterminada continua en V2.
- Contrato de toolchain: 46/46.
- Contrato E9/E10: 79/79.
- C++/CLI: 71/71.
- Documentacion/ejemplos de ownership: 41/41.
- `nsc.version`: 1/1 mediante filtro exacto.
- Metadata: la expectativa modificada de identidad aprobo; la suite conserva 13 fallos
  independientes ya observados en el baseline cancelado (NSMETA/ABI/fixtures historicos), que
  se corrigen en una tanda separada.
- No se ejecuto el baseline general.
