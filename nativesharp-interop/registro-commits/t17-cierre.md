# Tanda 17: cierre

## Criterio

El mismo de las tandas [10](t10-cierre.md) a [16](t16-cierre.md). Una casilla T17 se cierra cuando:

- el diagnóstico, la información de depuración o la reutilización que pide tiene evidencia propia en un
  fixture, frente a lo que hace Clang 23.1.0 con las mismas entradas, en biblioteca estática y DLL;
- cada forma que NSC no cubre queda registrada abajo como pendiente, con la tanda o el ítem que la continúa.

Un límite registrado no es interoperabilidad terminada (T18-007).

## Casillas

| Ítem | Capacidad comprobada | Registro |
|---|---|---|
| T17-001 | Rechazo de una importación con la declaración C/C++ y su archivo, línea y columna | [ubicaciones](t17-ubicaciones.md), [diagnósticos](t17-diagnosticos.md) |
| T17-002 | Cada uso inválido señalado en `Main.ns` con línea, columna y subrayado | [diagnósticos](t17-diagnosticos.md) |
| T17-003 | Una nota por candidato en aridad, tipos y ambigüedad, con su declaración y su motivo | [sobrecargas](t17-sobrecargas.md), [diagnósticos](t17-diagnosticos.md) |
| T17-004 | Notas de instanciación de Clang con su ubicación | [ubicaciones](t17-ubicaciones.md), [diagnósticos](t17-diagnosticos.md) |
| T17-005 | Encabezado y biblioteca ausentes con mensajes distintos; función no definida con su declaración | [símbolos](t17-simbolos.md), [diagnósticos](t17-diagnosticos.md) |
| T17-006 | Encabezado y biblioteca que discrepan en la firma, con la definición encontrada | [símbolos](t17-simbolos.md), [diagnósticos](t17-diagnosticos.md) |
| T17-007 | El PDB reúne las líneas de NativeSharp, del encabezado y de la biblioteca | [depuración](t17-depuracion.md), [prueba](t17-depuracion-prueba.md) |
| T17-008 | `Main`, `A::two` y `A::one` en el PDB con `-g` y `-gline-tables-only` | [depuración](t17-depuracion.md), [prueba](t17-depuracion-prueba.md) |
| T17-009 | La compilación se reutiliza solo con las mismas entradas; diez cambios frente a Clang | [caché](t17-cache.md) |
| T17-010 | Costo medido: 10,1 s con la STL frente a 0,5 s de Clang; 1,1 s reutilizado | [caché](t17-cache.md) |
| T17-011 | Caché de compilación para sesiones C++ con la captura completa de sus entradas | [caché](t17-cache.md) |
| T17-012 | Guía de declaración y uso de bibliotecas C/C++ | [guía](t17-guia.md) |

## Pendientes que quedan fuera de la tanda 17

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Alternativas en las llamadas a métodos de clases C++ | T17-003 | pendiente documentado |
| Sobrecargas C++ que solo difieren en `int` y `long` (ambiguas desde NativeSharp) | T17-003 | T18-021 |
| Ubicación C++ con la línea del encabezado y subrayado | T17-001 | pendiente documentado |
| Variables y miembros C++ que faltan al enlazar (solo el mensaje del enlazador) | T17-005 | pendiente documentado |
| `-fdebug-prefix-map` en el código C++ emitido | T17-008 | pendiente documentado |
| Costo del inventario de `names` y `members` sin reutilización (encabezados grandes) | T17-010 | T18-021 |
| Caché en `nsc build` para proyectos C/C++ | T17-011 | pendiente documentado |
| Depuración interactiva (solo se comprueba el PDB) | T17-007 | pendiente documentado |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
