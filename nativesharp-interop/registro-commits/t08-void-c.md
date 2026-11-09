# T08-008: `void` C

Fecha: 2026-11-08. Casillas: T08-008-02a–d. También incluye la validación del bloque T08-008.

## Resultado

Verificadas sin cambios en el compilador. Queda un hallazgo para T08-010-04b.

## Prueba

Nueva: `pruebas-tanda-08-void-c/validar-void.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

El mismo cuerpo se ejecuta en un control Clang, directo con el encabezado y en una plantilla de un productor
precompilado consumida sin encabezado. El camino precompilado es el que cubre 02d: las firmas se restauran
desde la metadata.

- **02a — retorno `void` y `f(void)`.**
  - `reset()`, `bump(int)` y `counter(void)`: llamadas por efecto y contador leído de vuelta.
  - Llamar a `counter(1)` o a `bump()` se rechaza: «no overload of namespace function '…' accepts N
    argument(s)».
  - Usar el resultado de `reset()` se rechaza: «cannot initialize local 'v' of type 'builtin:int' from
    'builtin:void'».

  Clang rechaza los tres.
- **02b — `void*`.**
  - Como parámetro, retorno, campo y global.
  - Ida y vuelta por C (`identity`, `read_int`, `write_int`), un búfer C leído como `int*` tras un cast y la
    asignación de `null`.
  - Desreferenciar `void*` se rechaza: «operand of * must be a non-void pointer».
- **02c — calificadores.**
  - `const void*` y `volatile void*` se aceptan en parámetro, retorno y local, y `const void*` también como
    campo. Añadir el calificador es implícito.
  - Quitarlo se rechaza:
    - «would discard pointee constness»;
    - «initialization would discard pointer pointee volatile qualification».

    Clang solo avisa en esos dos casos.

Validación del bloque: la prueba nueva y la muestra previa T08-008-01, que forma parte de las pruebas C11
del carril y no usa código cambiado. Ningún archivo del compilador cambió en este bloque.

## Hallazgo para T08-010-04b

Un campo de struct C `volatile void *vp` no se importa:

- en el camino directo da «receiver/type '…Holder' has no compatible member named 'vp'»;
- en el camino precompilado da «C record field cannot be reconstructed: …Holder.vp».

`projectCppSemanticInstanceField` excluye los punteros cuyo apuntado es `volatile`
(`!field.type.element->isVolatile`). El campo importado tampoco tiene dónde guardar esa calificación: solo
registra `pointeeReadonly`. Corresponde a «campos puntero … incluidos calificadores». Las globales y los
locales `volatile void*` sí funcionan.
