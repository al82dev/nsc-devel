# T08-013: tipos C incompletos

Fecha: 2026-11-08. Casillas: T08-013-02a–d y T08-013-03a–c. También incluye la validación del bloque T08-013.

## Resultado

Verificadas, con una corrección en el compilador: los campos `Record **` de records C. Queda la decisión pendiente
T08-013-02c1.

## Corrección: campo `Opaque **`

**Síntoma.** En `struct Holder { Opaque *p; Opaque **pp; }` el campo `pp` no se importaba:

- en el camino directo, «has no compatible member named 'pp'»;
- en el camino precompilado sin encabezado, «C record field cannot be reconstructed: OpPub::Holder.pp».

Los parámetros `Opaque **` sí funcionaban.

**Causa.** `projectCppSemanticInstanceField` solo resolvía un record apuntado a un nivel (`Record *`). Para
`Record **`, el apuntado es otro puntero, que `mapCppScalarType` no proyecta cuando la base es un record.

**Corrección.** Si los punteros intermedios no tienen calificadores, se recorren hasta la base. Si la base es un
record sin calificar, la clave del tipo conserva el record y agrega un `*` por cada nivel.

Un puntero intermedio calificado (por ejemplo, `Opaque * const *`) sigue sin importarse, como antes.

## Prueba

Nueva: `pruebas-tanda-08-incompletos-c/validar-incompletos.py`. Aprobada en O0–O3, con los rechazos en O0. C
comprueba cada valor.

- **02a:** struct declarado por adelantado y definido después en el mismo encabezado. Se usa por valor y en una
  lista enlazada; `sizeof` coincide con C.
- **02b:** union declarada por adelantado en `a.h` y definida en `b.h`, ambos del mismo proyecto. Se usa por
  valor y se lee desde C.
- **02c:** el tipo opaco publicado por un productor y el mismo tipo visto por el encabezado del productor en el
  consumidor son uno solo. El puntero de la biblioteca pasa por las funciones C del encabezado y vuelve igual.
- **02d:** dos definiciones incompatibles de `struct Thing` en un proyecto se rechazan al usarse: «referenced
  C/C++ record has conflicting or unverified definitions».
- **03a:** `Opaque *` y `Opaque **` como parámetro, retorno, local, campo y global. `pp[1]`, `*(pp + 1)` y
  `pp[2] = a` se admiten. También se ejecuta en una plantilla precompilada consumida sin encabezado.
- **03b:** rechazos.
  - `sizeof(Opaque)`: «C/C++ type 'Opaque' is incomplete and has no object layout».
  - Variable por valor: «cannot be created by value because its type is incomplete».
  - Acceso a miembro: «has no compatible member named 'v'».
- **03c:** `p + 1` y `p[1]` sobre `Opaque *` se rechazan: «pointer arithmetic requires a complete C type». La
  aritmética sobre `Opaque **` se admite (03a).

## Decisión pendiente T08-013-02c1

**Situación.** Cuando el consumidor tiene su propia copia de la declaración adelantada (`typedef struct Opaque
Opaque;` en otro archivo) y no el encabezado del productor, el tipo opaco no se reconcilia con el de la metadata:

- con typedef da «C/C++ type alias conflicts with a native type 'Opaque'»;
- con solo la etiqueta da «cannot initialize local 'o' of type 'type:C::Opaque*' from 'type:Pub::Opaque*'».

Con el mismo encabezado sí se reconcilia. Un struct completo en dos archivos distintos también se reconcilia,
por comparación estructural.

**Causa.** Para dos records incompletos, `compareCRecordSourceEvidence` exige la misma procedencia (archivo,
hash y offset): no hay estructura que comparar.

**Opciones.**

1. Reconciliar los records incompletos por etiqueta y USR. Es la regla de C dentro de una unidad de traducción,
   pero unifica tipos opacos homónimos de bibliotecas distintas.
2. Mantenerlos distintos, que es el comportamiento actual.

La sonda está en `A:\nsc-interop-build\claude\sondas\openarr\sonda24.py` (archivos distintos) y en `sonda24b.py`
(mismo archivo).

## Validación del bloque

- La prueba nueva.
- La muestra previa T08-013-01: `pruebas-tanda-08-incompleto/validar.cmd` y `validar-opaco.cmd`.
- La batería de records, uniones, layout, typedef, enum e identidad, con el compilador final (ver el commit).
