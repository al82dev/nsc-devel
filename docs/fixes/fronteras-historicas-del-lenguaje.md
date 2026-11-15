# Fronteras históricas del lenguaje verificadas en esta revisión

Fichas LKF-049, 053, 056, 062, 063, 065, 071, 077, 078, 080 y 082. Cada una citaba un comportamiento
de una distribución anterior. Las sondas equivalentes se ejecutaron con el compilador vigente;
donde había un defecto se corrigió, y donde la frontera sigue siendo una decisión de diseño se
verificó que tenga un diagnóstico explícito.

## Defectos corregidos

| Ficha | Antes | Ahora |
|---|---|---|
| LKF-053 | `static_assert` no plegaba campos `const`, constantes de enum ni `default(T)` ("not a compile-time bool constant"). | `resolveStaticAssertOperands` sustituye la constante de enum por su valor, con el tipo subyacente, y el campo `const` por su inicializador, transitivamente hasta profundidad 32. `default` de bool, entero o flotante es su cero. Un campo no `const` sigue siendo no constante. |
| LKF-062 | Las sobrecargas que solo difieren en `owned`/`borrowed` compartían símbolo: NSL1021 "duplicate symbol definition". | `mangleCallable` añade el calificador de cada parámetro solo cuando existe esa colisión. Ningún otro símbolo cambia de nombre. |
| LKF-063 | `ref int e = ref a[0]; delete[] a; return e;` compilaba y leía memoria liberada. | Se rechaza `delete[]` mientras un `ref` local a un elemento del arreglo sigue en ámbito. La regla es léxica, como la de vistas prestadas, y el control con el `ref` en un bloque cerrado ejecuta. |
| LKF-071 | `delete this` en un método ordinario compilaba. | Se rechaza: el método toma prestado su receptor y el dueño lo libera. |

## Verificado sin cambio

- **LKF-049.** `partial struct` y los genéricos parciales se rechazan con diagnóstico explícito; `partial class` funciona.
- **LKF-056.** Un desplazamiento constante fuera de rango atrapa (131), igual que uno en tiempo de ejecución. La asignación compuesta de propiedad exige un receptor reevaluable. La contradicción entre los capítulos 11/19 y 18/19 es documental: el comportamiento vigente coincide con el capítulo 11 para los desplazamientos y con el 19 para las propiedades.
- **LKF-065.** Un struct con campo `owned` es move-only: copiarlo se rechaza y `move` ejecuta.
- **LKF-077.** `++` y `--` de usuario, prefijo y sufijo, ejecutan con el valor correcto.
- **LKF-078.** Una union cruda con campo `owned` se rechaza; una union de valores ejecuta.
- **LKF-080.** Un método genérico dentro de un tipo genérico ejecuta; `export extern C` genérico se rechaza.
- **LKF-082.** Un constraint en una función global, un struct que implementa interfaz y `where T : U` se rechazan con diagnósticos explícitos. `where T : struct` funciona y rechaza una clase.

## Prueba

`nsc.compiler.historical-language-boundaries`: 34 casos; cada rechazo tiene un control aceptado y los aceptados salen 42 en O0/O2.

## Límites que quedan

- Un `const` local no es constante de compilación: el lenguaje permite inicializarlo en tiempo de
  ejecución. Por eso `static_assert` lo trata como no constante, que es lo que distingue la ficha.
- `sizeof(T)` produce `nuint` y no es constante para el evaluador de campos `const` ni para la
  conversión implícita. `int s = sizeof(long);` y `const int s = sizeof(long);` se rechazan, aunque
  `static_assert(sizeof(long) == 8u)` pliega.
