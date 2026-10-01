# Tanda correctiva G00: cierre de imports del autoalojado

## Problema

El runtime Win32 de NativeSharp 3.0 incorporó `HeapReAlloc` y `GetLastError`, pero los
verificadores SH41, SH43 y SH44 continuaban generando una biblioteca de importación de
`KERNEL32.dll` con el cierre anterior. El primer enlace de SH41 fallaba aunque la tabla
canónica usada por el compilador normal ya contenía ambos símbolos.

## Cambios

- `cmake/VerifyNativeSharpSelfHostedGeneration.cmake`: agrega ambos imports al cierre de SH41.
- `cmake/VerifyNativeSharpRepeatedFixedPoint.cmake`: mantiene SH43 alineado con el mismo runtime.
- `cmake/VerifyNativeSharpCrossMachineDeterminism.cmake`: mantiene SH44 alineado con el mismo runtime.

No se modificó el compilador, el runtime ni la instalación de `C:\native-sharp`.

## Mejora

Los ejecutables autoalojados se enlazan contra el cierre real del runtime 3.0. Esto elimina
los `undefined symbol: HeapReAlloc` y `undefined symbol: GetLastError` y evita que el mismo
desfase reaparezca en las comprobaciones posteriores de punto fijo y determinismo.

## Validación específica

- Comprobación textual de que SH41, SH43 y SH44 contienen ambos símbolos.
- Generación individual de las tres bibliotecas de importación mediante NSLink usando exactamente
  las listas declaradas por SH41, SH43 y SH44, con productos temporales exclusivamente en `A:`.
- La ejecución completa de SH41 quedó para el siguiente baseline: la comprobación dirigida fue
  cancelada por solicitud del usuario cuando comenzó el baseline integral.
- No se ejecutó el baseline general.
