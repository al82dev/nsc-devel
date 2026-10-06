# Compatibilidad del indexador de StringView

## Cambio

El indexador de `StringView` ya no publica `throws`. Una lectura fuera de rango devuelve el carácter nulo y
no accede a memoria ajena. `UncheckedAt` conserva el acceso explícito sin comprobación.

## Motivo

El compilador oficial usado para construir las bibliotecas debe aceptar el proyecto sin depender de contratos
`throws` en accesores de indexadores.

## Verificación

`NativeSharp.Core.nsproj` compiló en O0 con `C:\native-sharp\bin\nsc.exe`, build
`2a44eedad51a7a9b`. La emisión de `.lib`, `.h` y `.nsmeta` terminó correctamente.

No se ejecutó baseline ni otra optimización.
