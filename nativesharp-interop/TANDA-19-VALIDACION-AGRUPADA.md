# Validación agrupada de la tanda 19

Base: `73673726b`. El compilador incluye los siete cambios de la tanda hasta `c4c1df992`:

- [`null` en punteros a función](registro-commits/t19-nulos.md);
- [sobrecargas aritméticas](registro-commits/t19-sobrecargas.md);
- [retornos `const T&`](registro-commits/t19-referencias.md);
- [acceso](registro-commits/t19-acceso.md);
- [uniones](registro-commits/t19-uniones.md);
- [alias de namespace](registro-commits/t19-alias.md);
- [enumeradores y miembros estáticos por el alias](registro-commits/t19-alias-enumeradores.md).

## Interoperabilidad con enlace estático (2026-11-16)

`lote-t19.cmd t19i` ejecuta, con cinco procesos, todos los guiones `validar*.py` de `pruebas-tanda-19-*` a
`pruebas-tanda-09-*` y `lote-ct2.cmd`. Registros: `logs/regresion-t19-t19i.log` a `logs/regresion-t09-t19i.log` y
`logs/lote-ct2.log`.

| Tandas | Guiones | Resultado |
|---|---|---|
| 19 a 10 | 67 | los 67 pasan |
| 09 | 1099 | 1094 pasan, 3 fallan, 2 omitidos |

Las fallas de la tanda 09 son las de la línea base:

- `cpp-variadica-preserve-none` (`validar` y `validar-precompilado`), como en la
  [validación de la tanda 18](TANDA-18-VALIDACION-AGRUPADA.md);
- `cpp-tamano-objeto/validar-indice-expresion` falla con cinco procesos y pasa repetido con uno
  (`logs/regresion-t09-t19is`).

Los dos omitidos requieren `--caso` y `--mutador`.

`lote-ct2.cmd`: `ninja -k 0` termina con 0 errores de compilación y los 3 símbolos indefinidos conocidos
(`logs/t09-build-k0.log`). Las pruebas `clang-abi-probe`, `regcall-record-plan`, `variadic-pointer`,
`project-loader`, `item331` a `item333`, `item344`, `item345` e `identity` pasan.

## Interoperabilidad con DLL

`lote-dll19.cmd t19id` ejecuta con `--dll` los guiones que lo aceptan, de la tanda 19 a la 09. Registros:
`logs/regresion-dll-t19-t19id.log` a `logs/regresion-dll-t09-t19id.log`.

| Tandas | Guiones | Resultado |
|---|---|---|
| 19 a 09 | 67 | 66 pasan, 1 falla |

La falla es `pruebas-tanda-11-destructores/validar.py`, en O3, y la dio el control de Clang, no NativeSharp: el
`control.exe` tardó 30 s y devolvió 0 en lugar de 226. Lo comprobado:

- O0 a O2 pasaron;
- el mismo `control.exe`, ejecutado después, devuelve 226 en 36 ms;
- repetido con un proceso, el guion pasa completo, O0–O3, con su rechazo y su contraparte
  (`logs/regresion-dll-t19ir2`).

La causa de la primera ejecución no la he verificado. Un reintento intermedio
(`logs/regresion-dll-t19ir`) se ejecutó en paralelo con otra prueba y repitió el mismo síntoma.

## Estado de la tanda 19

Los siete cambios tienen su evidencia frente a Clang 23.1.0, en estática y DLL, O0–O3. Cada uno cubre una parte
de su tarea; lo que falta de cada una sigue en su casilla de la sección 19 de TRACKING:

| Tarea | Cubierto en esta tanda | Sigue pendiente |
|---|---|---|
| T19-002 | `null` en punteros a función C | dirección de un global `_Atomic` TLS; `new` sobre structs C; `sizeof` como `int` |
| T19-004 | campos de unions C++ de miembros escalares | unions con miembros record; bit-fields de tipo enum o de más de 64 bits |
| T19-005 | tipos, enumeradores y métodos estáticos por un alias de namespace | plantillas por el alias; alias y `using` de variables; `using` global; anidados privados |
| T19-006 | sobrecargas aritméticas de funciones de namespace | alternativas en métodos de clases C++; predeterminados no constantes |
| T19-007 | retornos `const T&` de funciones y métodos | `T&&`; referencias en métodos estáticos; operandos `T&` en operadores |
| T19-012 | acceso de campos y métodos de instancia | métodos virtuales o heredados no públicos |

La tanda queda abierta: ninguna de esas tareas está completa. No se modificó Clang ni NSLink. El contrato NSMeta
vigente es el único.
