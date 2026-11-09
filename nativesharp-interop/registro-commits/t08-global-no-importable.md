# T08-002: plantilla que nombra una global C que su productor no pudo importar

Fecha: 2026-11-08. Casilla: T08-002-09m7d2, «Rechazar el uso transportado de una global cuya declaración en
el productor no se pudo importar (`const int **`)».

## Reproducción

Caso `rechazo-puntero-doble` de `global-externa-variantes/validar.py --observar` (O0):

- El productor A ve `extern int **pp;` y el productor B ve `extern const int **pp;`.
- Las plantillas de los dos productores leen `pp`, y el consumidor no tiene sus encabezados.

El consumidor se construía y devolvía 1001: la plantilla de B se unía en silencio al `int **pp` de A. En C,
esas dos declaraciones son incompatibles. Clang: «redeclaration of 'pp' with a different type».

Causa: `const int **` aún no tiene proyección NativeSharp, porque falta un calificador anidado en el dato
apuntado. Al publicar, `captureCppTemplateGlobalCandidates` solo registraba en la metadata las globales con
proyección e ignoraba las demás sin dejar rastro. El consumidor no sabía que B había visto otra declaración y
solo encontraba la de A.

## Cambio

No hizo falta cambiar NSMeta. El catálogo de dependencias C ya guarda la declaración C completa de cada
global que usa una plantilla.

- `CppSemanticInventory.cpp`, productor: si la plantilla nombra una global C sin proyección, se registra la
  declaración que vio. Se exige una declaración externa C con símbolo y USR que el codec pueda codificar; si
  no, se conserva el comportamiento anterior.
- `CppSemanticRecord.cpp`, consumidor: si una global de la metadata no se puede proyectar, ya no es un error
  fatal de toda la compilación. Se crea un estado con `cppImportError`, igual que una global viva no
  importable. El análisis del cuerpo informa:
  - «cannot import C/C++ global 'pp': …» si es la única candidata;
  - «ambiguous namespace state reference 'pp'» si compite con la declaración de otro productor.

Efecto lateral: una global record o enum de una plantilla precompilada (límite registrado en
[t08-global-arreglo-incompleto](t08-global-arreglo-incompleto.md) para T08-004-04b) ya no aborta toda la
compilación del consumidor. Ahora se rechaza solo el uso, con el motivo de la importación.

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/global-externa-variantes/validar-global-no-importable.py`. Resultado:
aprobada.

- Control C con Clang: `int **pp` y `const int **pp` son redeclaraciones incompatibles.
- Rechazos (O0), en consumidores sin encabezados:
  - productores `int **` y `const int **`: «ambiguous namespace state reference 'pp'»;
  - solo el productor `const int **`: «cannot import C/C++ global 'pp'».
- Positivos en O0–O3, con un productor que ve la declaración no importable:
  - un campo estático público de la clase oculta la global;
  - un local de la plantilla oculta la global;
  - otra plantilla del mismo productor usa solo una global importable.
- `global-externa-variantes/validar.py --observar 0`: `rechazo-puntero-doble` ahora se rechaza. Esa prueba no
  se modificó.

Regresión dirigida (`claude/scripts/regresion-globales.cmd`):
- `externa-dos-bibliotecas` (dos validadores);
- `puntero-externo-dos-bibliotecas`;
- `static-dos-bibliotecas` y su variante mutable, también con TLS;
- `homonimos/validar-global-static.cmd`;
- `plantillas/validar-global-alias-o0.cmd`;
- `tls-dll-plantillas`;
- `global-externa-variantes/validar.py` y `validar-arreglo-incompleto.py` en O0 y O3;
- miembros flexibles C.

Todo se aprobó salvo `plantillas/validar-enum-conflicto-o0.cmd`, que falla sin relación con este cambio: el
consumidor que lee su propio `GenLoose` ya no ve la ambigüedad. Lo provocó la regla de enumeradores por
productor de `c9511d0ab` (interop-t08-typedef-parametro-enum). Este cambio no toca los enumeradores. Se
corrige por separado, dentro de la validación del bloque T08-002.

## Observación (para T08-018-03c)

La sonda `claude/sondas/openarr/sonda5.py` usa una plantilla pública que lee un campo estático **privado**
de su clase:

- Sin ninguna global C con ese nombre, el consumidor la rechaza con «unknown identifier 'pp'». El campo
  privado no viaja con la plantilla.
- Si el encabezado del productor declara además `extern int pp;` importable, la plantilla se une en silencio
  a la global C en lugar del campo. Falla al enlazar porque nadie define `pp`; si estuviera definida,
  leería otro objeto.

Esto no depende de este cambio: es la sombra de una global C desde una plantilla precompilada, y se registra
en T08-018-03c.
