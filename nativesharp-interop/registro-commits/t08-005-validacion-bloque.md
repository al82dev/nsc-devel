# Validación del bloque T08-005 (typedef C)

Fecha: 2026-11-08. Compilador del commit `b7eef09bc` (carril `tanda1-ON`).

## Resultado

Bloque aprobado. Todas las casillas de T08-005 están cerradas. Quedan dos decisiones pendientes anotadas en
el tracking: T08-005-03a1 y T08-005-03b1.

## Ejecución

Script: `A:\nsc-interop-build\claude\scripts\validar-bloque-t08-005.cmd`. Las pruebas `.cmd` se ejecutan
desde una copia CRLF de `nativesharp-interop`.

| Prueba | Resultado |
|---|---|
| `pruebas-tanda-08-typedef-c/validar-formas.py` (O0–O3 y rechazos) | correcto |
| `pruebas-tanda-08-typedef-c/validar-reconciliacion.py` (O0–O3 y rechazos) | correcto |
| `typedefs/precompiled-conflict`, `typedefs/precompiled-enum`, `typedefs/EnumConflict` | correcto |
| `nombres-metadata/validar-referenciados.cmd`, `meta-c/validar.cmd`, `incompleto/validar-opaco.cmd` | correcto |
| `typedef-funcion-record/validar.cmd` (incluye `--incompatible`, `--const-mismatch`, `--header-changed`) | correcto |
| `plantillas/validar-enum-conflicto-o0.cmd`, `plantillas/validar-global-alias-o0.cmd` | correcto |
| `typedef-calificadores/validar-callback-volatile.py`, `validar-delegate-alias-calificado.py` | correcto |
| `tls-dll-distintas/validar-variadicas-puntero.py` | correcto |
| `pruebas-tanda-08-identidad-c/validar.cmd` (copia CRLF) | correcto |
| `regresion-callbacks.cmd` | correcto, salvo el fallo conocido T08-017-02a |

## Nota de infraestructura

`correr.cmd` cambia al directorio `A:\nsc-devel`, así que una ruta relativa ejecuta el original. Cuando el
original es `pruebas-tanda-08-identidad-c/validar.cmd`, llama a `externa-dos-bibliotecas/validar.cmd`, que
solo tiene finales LF, y cmd no encuentra la etiqueta `:setup_atomic`.

El validador de identidad se ejecuta por eso con ruta absoluta a la copia CRLF (`identidad-crlf.cmd`). Es el
mismo problema ya anotado como decisión de infraestructura.
