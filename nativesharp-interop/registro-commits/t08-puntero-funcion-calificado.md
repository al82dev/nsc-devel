# T08-002: punteros a función C sin typedef con datos apuntados `const` o `volatile`

Fecha: 2026-11-08. Pendiente que se cierra: «Importar funciones, campos y globales C cuyo tipo es un
puntero a función con parámetros `volatile T*` o `const T*`» (casilla de
[t08-callback-volatile-identidad](t08-callback-volatile-identidad.md), ampliada a `const` en
[t08-callback-const-identidad](t08-callback-const-identidad.md)).

## Reproducción

La sonda `claude/sondas/fnptr1` usa un encabezado directo en O0. Antes del cambio, NSC no importaba
ninguna de estas declaraciones:

| Declaración C | Síntoma |
|---|---|
| `int apply_const(int (*cb)(const Payload*), const Payload*)` | `unknown identifier 'apply_const'` |
| `int apply_volatile(int (*cb)(volatile Payload*), volatile Payload*)` | ídem |
| `int (*const_callback(void))(const Payload*)` y la variante `volatile` | `unknown identifier` |
| `const Payload* (*const_getter(void))(void)` | `unknown identifier` |
| campo `int (*cb)(const Payload*)` de un struct | `has no compatible member named 'cb'` |
| global `int (*global_const)(const Payload*)` | `C/C++ global type is not yet mapped` |

Causa: la rama de puntero a función de `mapFunctionType` (`CppSemanticFunction.cpp`) rechazaba cualquier
parámetro o resultado cuyo dato apuntado tuviera `const` o `volatile`. El comentario de la propia rama
lo explicaba: «The structural function key does not encode pointee const». Desde los dos commits
anteriores, la identidad `fn:` sí registra esas calificaciones.

## Cambio

En la rama de puntero a función de `mapFunctionType`, para el resultado y para cada parámetro:

- Si el valor es un puntero cuyo dato apuntado es escalar, enum o record, el portador se mapea sin los
  calificadores de ese dato. Los punteros a puntero, a función y a arreglo quedan fuera.
- `parameterPointeeReadonly` / `parameterPointeeVolatile` y `resultPointeeReadonly` /
  `resultPointeeVolatile` registran esos calificadores en la identidad.

Así, una función, un campo o una global con ese tipo obtienen la misma identidad que el typedef
equivalente. Las comprobaciones de los commits anteriores (conversión, invocación, resultado y `var`) se
aplican sin más cambios. Cualquier otra calificación sigue rechazándose como antes.

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/typedef-calificadores/validar-puntero-funcion-calificado.py`. Usa
una DLL C con declaraciones `dllimport`; en el encabezado no hay ningún typedef de función. Resultado:
aprobada.

- Control C con Clang:
  - Los ocho casos positivos devuelven los valores esperados.
  - Pasar `take_volatile` donde se espera `int (*)(const Payload*)`, o al revés, y asignarlo a un campo
    son errores («incompatible»).
  - Escribir mediante el resultado del getter `const` es un error («const-qualified»).
- Cinco rechazos, cada uno directo con encabezado y precompilado consumido sin encabezado:
  - un argumento de función con la calificación opuesta, en las dos direcciones;
  - una escritura mediante el resultado `const`;
  - un argumento `volatile` a un callback devuelto con parámetro `const`;
  - una asignación incompatible a un campo.
- Ocho positivos en O0–O3, directos y precompilados sin encabezado: parámetro `const` y `volatile`;
  resultado de función que devuelve un callback `const` y `volatile`; getter `const` y `volatile`; campo
  con dos callbacks; dos globales.

## Hallazgo registrado aparte

En la primera versión de la prueba, el struct se declaraba solo con etiqueta
(`struct Holder { … };`, sin typedef). En el camino precompilado sin encabezado, el consumidor informó
`unknown type name 'Holder'`. Con un struct sin calificadores (un callback `int (*)(Payload*)` y un
`int`), y aun accediendo solo al `int`, el error se repite; por eso no depende de las calificaciones. No
se comprobó con el compilador anterior a esta serie de cambios. La prueba usa `typedef struct Holder {…} Holder;`. El caso se anota
como pendiente nuevo, T08-010-10.
