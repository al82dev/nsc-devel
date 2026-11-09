# T08-018: colisiones de nombres C

Fecha: 2026-11-08. Casillas: T08-018-02a–d y T08-018-03a–c. También incluye la validación del bloque T08-018.

## Resultado

- **Verificadas sin cambios en el compilador:** T08-018-02a–d, 03a y 03b.
- **T08-018-03c:** verificada para la función, la global y el enumerador C, y para un campo público homónimo.
- **Decisión pendiente nueva:** T08-018-03c1. Un miembro privado usado por una plantilla publicada no se
  publica, y en el consumidor el nombre se une en silencio a la declaración C homónima.

## Prueba

Nueva: `pruebas-tanda-08-colisiones-c/validar-colisiones.py`. Aprobada en O0–O3, con los rechazos en O0. Cada caso
positivo compara en NativeSharp valores calculados por C (`counter = 40`, `twice(v) = 2 * v`, `High = 9`).

- **02a — función C frente a función NativeSharp.**
  - `Mine.F.twice(2)` (NativeSharp) y `twice(2)` (C) son llamables distintos y devuelven 6 y 4.
  - Dentro de una clase, un método miembro `twice(int, int)` oculta la función C para cualquier aridad, como en la
    búsqueda por ámbitos de C#: `twice(1)` se rechaza con «no overload of 'twice' accepts 1 argument(s)».
- **02b — global y enumerador ocultados.**
  - Un local y un parámetro llamados `counter` y `High` ocultan la global y el enumerador solo en su ámbito.
  - Un campo estático de la clase oculta la global C dentro de la clase. Fuera, `counter` vuelve a ser la global.
- **02c — typedef C frente a tipo NativeSharp.**
  - `Mine.Gauge` (NativeSharp) y `Gauge` (typedef C) conviven y conservan sus layouts.
  - Un `struct Gauge` NativeSharp en el mismo namespace global se rechaza: «C/C++ type alias conflicts with a native
    type 'Gauge'».
- **02d — bucles, `catch` y lambdas.** Sombra por la variable de un `for`, por la variable de un `catch` y por el
  parámetro de una lambda. Una lambda sin sombra captura la global (`v + counter`). Después de cada ámbito,
  `counter` vuelve a valer 40.
- **03a — antes, dentro y después.** Lecturas de la global y del enumerador antes del bloque de sombra, dentro y
  después, con `read_counter()` confirmando que C no cambió.
- **03b — ambigüedad entre declaraciones C.**
  - Dos proyectos referenciados publican `extern int dup` y `extern long long dup`: «ambiguous namespace state
    reference 'dup'».
  - Un mismo proyecto importa dos encabezados con esas declaraciones: «cannot import C/C++ global 'dup':
    incompatible C/C++ global declarations».
- **03c — plantilla precompilada sin encabezado.** La plantilla lee el campo público `counter` de su clase (7, no
  la global C), llama a la función C `twice`, lee el enumerador `High` y oculta el campo con un local dentro de un
  bloque. El consumidor no tiene el encabezado.

## Decisión pendiente T08-018-03c1

**Situación.** Una plantilla pública que usa un miembro estático **privado** de su clase no funciona en el
consumidor, porque el miembro privado no se publica (regla DLL-P4 de `MetadataSourceMapping.cpp`: solo se exportan
miembros públicos y protegidos):

- sin declaración C homónima, el consumidor rechaza la instanciación: «unknown identifier 'other' in
  callable/initializer context»;
- con una global o función C homónima visible desde el productor, la plantilla se une **en silencio** a la
  declaración C. El campo privado `counter = 7` lee la global C (40) y el método privado `twice` llama a la función
  C (2 en vez de 7).

Un campo público homónimo sí se resuelve bien (7). El caso es general de plantillas y miembros privados; las
colisiones con C lo vuelven silencioso.

**Opciones.**

1. Publicar en NSMeta los miembros privados que usan las plantillas publicadas (cambia el contenido de NSMeta).
2. Rechazar en el productor una plantilla publicada que use miembros no publicables (cambia el lenguaje visible).
3. Mantenerlo, documentado como limitación.

Las sondas son `A:\nsc-interop-build\claude\sondas\openarr\sonda27.py` (bits por condición) y `sonda27b.py`
(campo público, campo privado, campo privado sin colisión y método privado).

## Validación del bloque

`A:\nsc-interop-build\claude\scripts\validar-bloque-t08-018.cmd`, con resultado 0:

- la prueba nueva;
- las muestras previas T08-018-01 y de sombras: `sombras-plantilla-c/validar-ambito.cmd`,
  `validar-o0-inicializadores.cmd`, `validar-o0-sombras.cmd` e `identidad-c/sombra-funcion/validar.cmd`.

Ningún archivo del compilador cambió en este bloque.
