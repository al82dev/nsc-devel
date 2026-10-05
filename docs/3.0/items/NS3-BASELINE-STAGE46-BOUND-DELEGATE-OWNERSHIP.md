# NS3 baseline: ownership del receptor en delegates precompilados

## Problema

El productor principal de Stage 46 devolvía un método ligado a un parámetro `borrowed`. El delegate
podía sobrevivir a la llamada y, por tanto, al préstamo del receptor. El frontend lo rechazaba antes
de producir `NativeMath.lib`; las comprobaciones posteriores fallaban en cascada por la ausencia del
artefacto.

## Cambio

`MakeInstance` recibe ahora el receptor como `owned`, crea el método ligado mediante
`move(target.Add)` y los dos consumidores transfieren su variable con `move(target)`. El primer
assert de Stage 46 incluye además stdout/stderr para que una futura falla del productor conserve su
diagnóstico primario.

## Archivos

- `tests/unit/Stage46NativeLibrariesTests.cpp`

## Mejora

La fixture representa el contrato real del delegate: el closure conserva la vida del receptor y es
su único propietario. Stage 46 deja de depender de un borrow escapado y los fallos derivados pueden
evaluarse contra artefactos realmente emitidos.
