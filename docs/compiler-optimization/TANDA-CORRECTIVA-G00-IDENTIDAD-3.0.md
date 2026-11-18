> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Tanda correctiva G00: identidad coherente de NativeSharp

## Problema

En esa campaña, la rama y el proyecto CMake identificaban al compilador con una etiqueta
distinta de la publicada para el lenguaje. La contradiccion aparecia en
`--print-compiler-identity`, el servicio de lenguaje, el contrato del toolchain y varias pruebas.
Ademas, `nsc.version` exigia un banner historico que ya no corresponde al formato real de consola.

**Contrato vigente:** el producto es `1.0.0`; el lenguaje es único y no se identifica ni
selecciona mediante una versión o una propiedad de proyecto.

## Cambios históricos de la campaña

- `include/nativesharp/Basic/Version.h`: aquella campaña alineó las identidades publicadas.
  Actualmente identifica únicamente la versión del producto.
- `CMakeLists.txt`: `nsc.version` valida el banner real y deriva la version desde `project()`,
  en vez de congelar texto 2.0 y un formato SemVer que el banner no imprime.
- `src/Driver/Driver.cpp`: aquella campaña ajustó la ayuda a la identidad de su compilador.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: los objetos generados registran la identidad del productor.
- Las pruebas de identidad, driver, servicio de lenguaje, metadata, C++/CLI, ownership,
  toolchain y politica de compatibilidad se ajustaron a la identidad de aquella revisión.
- Las pruebas de contratos derivan Native ABI, Runtime ABI, runtime symbol set y MIR schema
  desde sus constantes canonicas. Esto elimina expectativas obsoletas `runtime_abi=1.0`,
  `runtime_symbol_set=2` y `mir_schema=1.14` sin debilitar la verificacion de presencia.

No se modificó `C:\native-sharp` ni se promovió el producto en esa campaña.

## Mejora

La identidad actual del producto utiliza este formato:

```text
compiler_version=1.0.0+build.<timestamp>.<hash>
compiler_release=1.0.0
```

El contrato generado conserva simultaneamente los valores fisicos vigentes de ABI y MIR, y
la prueba de banner ya comprueba la salida que realmente ofrece el compilador.

## Validacion especifica

- Construccion exclusiva del target `nsc` en `A:\nsc-devel\build-clang`.
- Se inspeccionó la identidad del ejecutable de aquella revisión; no valida la identidad actual.
- Identidad CC1: 9/9.
- Comando de identidad: aprobado.
- Driver: 230/230.
- Servicio de lenguaje: aprobado en la revisión histórica.
- Contrato de toolchain: 46/46.
- Contrato E9/E10: 79/79.
- C++/CLI: 71/71.
- Documentacion/ejemplos de ownership: 41/41.
- `nsc.version`: 1/1 mediante filtro exacto.
- Metadata: la expectativa modificada de identidad aprobo; la suite conserva 13 fallos
  independientes ya observados en el baseline cancelado (NSMETA/ABI/fixtures historicos), que
  se corrigen en una tanda separada.
- No se ejecuto el baseline general.
