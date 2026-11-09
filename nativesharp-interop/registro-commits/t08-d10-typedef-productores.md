# T08-D10: typedef C homónimos e incompatibles de productores distintos

Fecha: 2026-11-09. Casillas: T08-D10 y T08-005-03b1 ([decisión anterior](t08-typedef-c.md#decisiones-pendientes)).

## Problema

Dos bibliotecas precompiladas pueden ver encabezados distintos que declaran typedef homónimos e incompatibles. Por
ejemplo, `score_t` como `int` en una y como `long long` en la otra, o `Box` y `Callback` sobre structs distintos. Un
consumidor que usaba las plantillas de las dos se rechazaba con «conflicting C/C++ type alias», aunque cada plantilla
solo necesitaba el typedef de su productor.

## Causa raíz

`ProjectNameLookup::expandCppTypeAlias` reunía las candidatas de todos los módulos visibles para el proyecto dueño
de la sintaxis, y un módulo precompilado ve a todos los demás de la compilación. Las constantes enum C y las globales
`static` ya resolvían en el catálogo de su productor (`cppFromPrecompiledCatalog`); los typedef no.

## Cambio

- `CppImportedTypeAlias.h`: nueva marca interna `fromPrecompiledCatalog`. No es NSMeta ni contrato público.
- `CppSemanticInventory.cpp`: `provideCppTypeAliasesWithPrecompiledCatalogs` pone esa marca en los alias
  reconstruidos del catálogo de dependencias C de una biblioteca precompilada.
- `QualifiedTypeNames.cpp`, en `expandCppTypeAlias`: si todas las candidatas del proyecto dueño vienen de su catálogo
  precompilado, la búsqueda se limita a ese catálogo. Es el caso de una plantilla transportada. Entonces:
  - los typedef homónimos de otras bibliotecas no son candidatos;
  - tampoco lo son los records C importados por otras bibliotecas. Ejemplo: el struct anónimo `Box` que nombra el
    typedef del otro productor.

  El código de un proyecto vivo conserva la ambigüedad, tenga o no encabezados propios, igual que con las constantes
  enum.

## Pruebas

**Nueva:** `pruebas-tanda-08-typedef-c/validar-typedef-productores.py`. Aprobada en O0–O3, con rechazos en O0.

- Productores X e Y, cada uno con su encabezado:
  - `score_t`: `int` en X y `long long` en Y;
  - `Box`: struct anónimo con un miembro `int` o `long long`;
  - `Callback`: puntero a función sobre `Box *`.
- Cada productor publica:
  - una plantilla con locales `score_t` y `Box`, `sizeof`, llamadas C por valor y un `Callback` obtenido de C e
    invocado desde C;
  - un método no genérico `Twice(score_t)`.
- **Casos positivos**, con un control Clang, en dos formas:
  - un consumidor sin encabezados usa los dos productores;
  - un consumidor con su propio encabezado, que da a `score_t` otro tipo (`short`), también los usa.
- **Rechazos:**
  - el código propio del consumidor que usa `score_t` o `Box`, sin encabezado y con encabezado propio;
  - un proyecto vivo que referencia los dos encabezados.

**Actualizadas.** Fijaban el rechazo anterior; lo autoriza el punto 6 del usuario.

| Prueba | Antes | Ahora |
|---|---|---|
| `typedef-c/validar-reconciliacion.py`, caso b con productores separados | rechazo «conflicting C/C++ type alias 'score_t'» (solo O0) | el consumidor se ejecuta, O0–O3 |
| `identidad-c/typedef-funcion-record/validar.py` con `--incompatible`, `--same-size-incompatible`, `--const-mismatch` y `--header-changed` | rechazo «conflicting C/C++ type alias 'Callback'» | el consumidor se ejecuta, como las variantes compatibles |
| `identidad-c/typedef-calificadores/validar.py`, casos `volatile_missing` y `volatile_removed` (dos productores con `Callback` sobre punteros con distinto calificador) | rechazo «conflicting C/C++ type alias 'Callback'» | el consumidor compone los dos productores y se ejecuta; el control Clang sigue comprobando que los dos typedef son incompatibles en una unidad |
| `identidad-c/tls-dll-distintas/validar-variadicas-puntero.py --mismatch` (`VarFn` con `int` en un productor y `long long` en otro) | rechazo «conflicting C/C++ type alias 'VarFn'» | sigue rechazado, ahora por el error de tipos propio del productor 2: `VarFn fn = var_read;` asigna una función `(int, ...)` a un `VarFn` `(long long, ...)` («no method in the method group exactly matches delegate signature») |

Con `--header-changed`, el encabezado de un productor cambia después de publicarse. El consumidor no lo lee: usa el
catálogo publicado.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d10.cmd`. Primera ejecución: dos fallos, las pruebas que fijaban el
rechazo anterior (`typedef-calificadores/validar.py` y `validar-variadicas-puntero.py --mismatch`, esta dentro de
`identidad-c\validar.cmd`). Se actualizaron como indica la tabla y se volvieron a ejecutar con resultado 0, junto con
`nombres-metadata/validar-referenciados.cmd`. Contenido de la regresión:

- las suites de typedef: la nueva, `validar-reconciliacion` y `validar-record-dos-encabezados`;
- `typedef-funcion-record/validar.py` en todas sus variantes, O0 y O3;
- la batería `identidad-c\validar.cmd`, que incluye typedefs, plantillas y homónimos;
- `nsc_cpp_session_tests`;
- el conjunto de D02.
