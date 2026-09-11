# 1.0.1 — Selección por alcance de runtimes precompilados

## Resultado

Una `LibraryReference` puede declarar el triplete `.lib`, `.nsmeta` y DLL sin
obligar al consumidor a importar o empaquetar esa DLL cuando sólo usa un
`extern C from "..."` reconstruido desde la metadata.

El triplete completo continúa validándose al cargar la referencia. La DLL se
activa únicamente cuando el MIR contiene una llamada real a un símbolo
NativeSharp o a un export C implementado por el productor.

## Causa raíz

`appendValidatedLibraryReferences` añadía incondicionalmente cada `Runtime` a
la lista final. Además, las declaraciones importadas presentes en MIR se
trataban como uso aunque no fueran invocadas. Una clase precompilada podía
aportar la declaración de `GetCurrentThreadId` desde `kernel32.dll` y, sin
embargo, el ejecutable terminaba obligado a importar la DLL NativeSharp del
productor.

## Archivos e implementación

- `src/Driver/Driver.cpp` conserva un mapa entre módulo NSMETA y runtime ya
  validado.
- Recorre cuerpos MIR e inicializadores de campos para reunir los `calleeName`
  que realmente producen referencias en el módulo.
- Selecciona la DLL del productor sólo para llamadas NativeSharp alcanzadas o
  para exports C publicados por ese productor.
- Los imports extranjeros reconstruidos, como `kernel32.dll`, continúan
  enlazándose mediante su import library y no seleccionan el runtime
  NativeSharp que únicamente transportó su contrato.

No se modificaron bindings ni tutoriales.

## Evidencia dirigida disponible

El reproductor canónico
`bindings/_bugs/precompiled_library_extern_c` se emitió en O0 con
`packaged-runtimes: 0`; el ejecutable importó `KERNEL32.dll`, `KERNELBASE.dll`
y `SHELL32.dll`, y terminó con código 42.

No se ejecutó baseline.

## Commit de referencia

### Español

**Título:** Empaqueta sólo runtimes precompilados alcanzados

**Descripción:** Mantiene la validación completa de cada triplete
precompilado, pero selecciona la DLL únicamente cuando el MIR contiene una
llamada real a un símbolo del productor. Evita exigir runtimes NativeSharp para
imports `extern C` extranjeros reconstruidos desde NSMETA.

### English

**Title:** Package only reached precompiled runtimes

**Description:** Keep full validation of every precompiled triplet while
selecting its DLL only when MIR contains an actual call to a producer symbol.
Avoid requiring NativeSharp runtimes for foreign `extern C` imports rebuilt
from NSMETA.

## English summary

Precompiled runtime packaging is now reachability-driven. Metadata may carry a
foreign C contract without forcing the producer DLL into the final executable,
while NativeSharp calls and producer-owned C exports still select that runtime.
