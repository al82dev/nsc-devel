# Tanda 17, cambio 4: depuración del código C++ que emite NSC

## Defecto

En la [auditoría inicial](../TANDA-17-AUDITORIA-INICIAL.md), con `-g` el PDB tiene `Main.ns` y `api.cpp`
(de la biblioteca compilada con `-g`) y los procedimientos `Main` y `A::one`, pero no la función inline
`A::two` que NSC emite desde el encabezado.

Causa en el código: `emitCppInlineIR` crea el generador de código de Clang con las `CodeGenOptions` de la
sesión, que no piden depuración; el nivel `-g` de NSC no llega a la emisión C++.

## Diseño

- `emitCppInlineIR` recibe el nivel de depuración (`CppDebugInformation`) y, si no es `None`, pide a Clang
  CodeView con columnas, en tablas de líneas o completo, sobre una copia de las opciones de la sesión.
- El driver le pasa el nivel de `-g`/`-gline-tables-only`. El enlazador fusiona ese CodeView en el PDB del
  programa como el de cualquier objeto.
- Relación entre ubicaciones (T17-007): el PDB del programa reúne las líneas de `Main.ns` (NativeSharp), de
  la biblioteca (`api.cpp`, si se compiló con `-g`) y del encabezado (`api.h`, las funciones que emite NSC).
  El depurador pasa de un lenguaje al otro siguiendo esas tablas; no hace falta una correspondencia propia.

## Cambio

- `CppInteropSession.h`: `CppDebugInformation`; parámetro de `emitCppInlineIR`.
- `ClangSession.cpp`: CodeView en las definiciones inline cuando se pide.
- `DriverArtifactGeneration.cpp`: el nivel de depuración llega a `appendCppInlineObjects`.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture `pruebas-tanda-17-depuracion` (registro de [depuración](t17-depuracion-prueba.md)). Sonda con el
compilador del cambio (`A:/temp/nsc-t17-sonda-y4droiwm`): el PDB tiene `Main.ns`, `api.cpp` y `api.h`, y los
procedimientos `Main`, `A::one` y `A::two`.

## Límites

- `-fdebug-prefix-map` no se aplica a las rutas del código C++ emitido.
