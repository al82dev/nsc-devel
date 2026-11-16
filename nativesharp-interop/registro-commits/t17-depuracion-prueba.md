# T17-007 y T17-008: información de depuración entre NativeSharp y C++

## Comprobación

Este cambio comprueba la depuración del [cambio 4](t17-depuracion.md); no cambia código de producción.

## Evidencia

Fixture nuevo `pruebas-tanda-17-depuracion/validar.py`, con y sin `--dll`. `Main` NativeSharp llama a
`A::one` (biblioteca compilada con `-g -gcodeview`) y a `A::two` (inline en el encabezado, emitida por NSC).
Con `-g` y `-gline-tables-only`, O0 y O2, NSLink y lld-link, el PDB del programa tiene `Main.ns`, `api.h` y,
en estática, `api.cpp`, y los procedimientos `Main`, `A::two` y, en estática, `A::one`, como el PDB del
control de Clang 23.1.0 tiene `api.h`, `api.cpp`, `A::two` y `A::one`. El resultado es 10 en los dos. Con
`--dll`, el PDB de la DLL tiene `api.cpp` y `A::one`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0, O2; `-g`, `-gline-tables-only`) | `A:/temp/nsc-t17-depuracion-vvffj4tu` |
| DLL | pasa (O0, O2; `-g`, `-gline-tables-only`) | `A:/temp/nsc-t17-depuracion-yol891_j` |

Antes del cambio 4 el PDB no tenía `A::two` ni `api.h` ([auditoría](../TANDA-17-AUDITORIA-INICIAL.md)).

## Límites

- No se ejecutó un depurador interactivo; la comprobación es la del contenido del PDB.
