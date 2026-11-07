# T08-002: global C `static` de encabezado y su relación con T08-004

Estado: **DOCUMENTACIÓN, SIN CAMBIOS**. La importación de variables globales C es
el carril T08-004. Este documento solo registra lo que encontró el carril de
identidad y no propone código para ese carril.

## Caso encontrado

```c
/* dirA/same.h y dirB/same.h */
static int c_same_static = 3;
/* dirC/same.h */
static int c_same_static = 4;
```

HECHOS COMPROBADOS (compilador de este carril, Clang 23.1.0):

1. Identidad Clang. La sonda 1 da `c:a.h@cid_static_global` para `a.h` y
   `c:b.h@cid_static_global` para el mismo texto en `b.h`. La sonda 5 da
   `c:same.h@c_same_static` en `dirA`, `dirB` y `dirC`, aunque `dirC` tiene otro
   valor. El USR usa solo el nombre base del archivo: no distingue directorios
   ni definiciones.
2. Importación. NSC no importa un global con enlace interno. `int Main() { return
   c_same_static - 3; }` con un solo encabezado (`StaticA`) da `unknown
   identifier 'c_same_static'`. Con dos encabezados (`StaticAB`, `StaticAC`) da
   el mismo error. Experimentos en
   `build-claude-t08/exp`, fuera del repositorio.
3. Selección. La sonda 2 informa `candidate=no conflict=''` para
   `cid_static_global` de `a.h` y `b.h`.

Causa, por lectura del código:

- `projectGlobalWithLookup` exige `externalLinkage` y un símbolo externo
  (`CppSemanticInventory.cpp:187-189`).
- La selección separa las declaraciones internas en `internalIndices`
  (`CppSemanticInventory.cpp:381-384`).
- `availableNames` solo registra nombres con enlace externo
  (`CppSemanticInventory.cpp:488-490`), así que la demanda no crea estado y la
  búsqueda no informa la razón.

## Relación con T08-004

T08-004 ("Importar declaraciones de variables globales C") tiene evidencia de
globales con enlace externo: escalares, `volatile`, `_Bool`, atómicos y TLS
(`TRACKING.md`, T08-004). No encontré en `TRACKING.md` evidencia ni decisión
sobre globales `static` de encabezado. "TLS estático" y "TLS interno" se
refieren al modelo TLS, no al enlace interno de C.

Lo que este carril aporta a T08-004, sin decidir por él:

1. Semántica C: cada unidad de traducción que incluye el encabezado tiene su
   propia variable. No hay símbolo que enlazar. Usarla desde NativeSharp exigiría
   materializar una copia propia, como ya ocurre con las funciones
   `static inline`, que se compilan por proyecto y referencia con
   `cppCInlineLinkSymbol` (`CppSemanticInventory.h`).
2. Identidad: el USR no sirve para unir ni para distinguir. Dos encabezados
   homónimos con valores distintos dan el mismo USR. Cualquier identidad debe
   incluir el proyecto y la referencia, igual que el símbolo interno de
   `static inline` (hallazgo H2 de la auditoría).
3. Diagnóstico: hoy el usuario ve `unknown identifier`. T08-004 decide si es el
   comportamiento deseado o si hace falta un mensaje explícito, por ejemplo
   "C global with internal linkage is not imported".
4. Precompilado: un global `static` usado por un cuerpo ya compilado viaja dentro
   de ese cuerpo. Usado por una plantilla pública, tendría el mismo problema que
   `static inline` en `ConsumerInline` (T08-002-PROPUESTA-METADATA-C.md).

## Archivos que tocaría una decisión de T08-004

`src/Interop/Cpp/CppSemanticInventory.cpp` (proyección y selección de globales) y,
si se materializa la copia, la emisión de definiciones C internas. Pertenecen al
carril de variables globales; este carril no los modificó.

## Pruebas existentes de este carril que cubren el caso

- Sondas 1 y 5 de `nsc_cpp_c_identity_tests`: USR dependiente del nombre base y
  único para homónimos (afirmado).
- Sonda 2: el global `static` no es candidato (observación).
