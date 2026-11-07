# T08-002: constantes de enum C en bibliotecas (hallazgo H7)

Estado: **REPRODUCIDO; CORRECCIÓN PROPUESTA Y MEDIDA, NO APLICADA EN LA RAMA**.
El parche está en
[`parches/t08-002-h7-excluir-constantes.patch`](parches/t08-002-h7-excluir-constantes.patch).

## Reproducción autocontenida

Fixture `pruebas-tanda-08-identidad-c/constantes-enum` (tablas `casos.txt` e
`inspeccion.txt`). El encabezado `Const.h` no necesita biblioteca C:

```c
enum CConstMode { CConstModeA = 10, CConstModeB = 11 };
enum { CConstLoose = 12 };
```

Cada productor tiene un único método público. Cada consumidor no tiene
encabezado propio y llama a ese método.

| Productor | Uso de la constante | Enum en la superficie | Productor hoy |
|---|---|---|---|
| `PConstNone` | ninguno (control) | no | construye; NSMETA sin estados ni catálogo |
| `PConstSurfaceOnly` | ninguno; publica `CConstMode Echo(CConstMode)` (control) | sí | construye; el catálogo contiene `CConstMode` con `CConstModeA=10` y `CConstModeB=11` |
| `PConstNamedPlain` | `return (int)CConstModeB;` en método ordinario | no | `metadata contains an incomplete or duplicate namespace state` |
| `PConstNamedTemplate` | lo mismo en una plantilla pública | no | mismo error |
| `PConstNamedSurface` | método `CConstMode Current()` y plantilla que usa `CConstModeB` | sí | mismo error |
| `PConstAnonPlain` | `return CConstLoose;` en método ordinario | no aplica (anónimo) | mismo error |
| `PConstAnonTemplate` | lo mismo en una plantilla pública | no aplica | mismo error |

El modo `--inspect-c-constants` de `nsc_cpp_c_identity_tests` lee el NSMETA de
cada productor que se construyó en la ejecución y comprueba tres cosas:

- no hay ningún estado de espacio de nombres sin espacio de nombres;
- no hay ningún estado con el nombre de la constante;
- el catálogo C contiene el enumerador requerido.

## Causa

Por lectura del código:

1. `provideCppSemanticEnumConstants` (`CppSemanticInventory.cpp:848-920`) crea
   cada constante de enum C demandada como `NamespaceStateSymbol` público, con
   `isCppEnumConstant`, espacio de nombres vacío y tipo `builtin:int` (el tipo
   del enumerador, no el del enum). C no tiene espacios de nombres, y las
   constantes C++ no se proyectan (`projectCppEnumConstant` rechaza `isCxx`).
   Por tanto **toda** constante de este tipo tiene el espacio de nombres vacío.
2. `appendNamespaceStateMetadata` (`LibraryMetadataConstruction.cpp:2961-2981`)
   publica los estados públicos del proyecto raíz. Excluye `isPrecompiled` e
   `isCppExternal`, pero no `isCppEnumConstant`.
3. La relectura del productor (`LibraryMetadataVerification.cpp:1371-1384`)
   exige un espacio de nombres, un nombre de almacenamiento y un nombre de
   acceso, y rechaza el estado.

Consecuencia: hoy no se puede construir **ninguna** biblioteca cuyo código use
una constante de enum C. Por eso ningún NSMETA válido existente contiene estos
estados.

## Corrección propuesta (parche A)

Una condición en `appendNamespaceStateMetadata`:

```diff
-            state.isPrecompiled || state.isCppExternal)
+            state.isPrecompiled || state.isCppExternal || state.isCppEnumConstant)
```

más un comentario bilingüe. Es el único cambio y está en el parche.

Por qué es correcta:

- El valor de la constante ya está plegado en los cuerpos compilados. Un estado
  publicado no aporta almacenamiento ni identidad: no hay símbolo de acceso y
  `__nsc_get_<storage>` no existe.
- El consumidor ya obtiene las constantes de un enum C desde el catálogo
  (`reconstructCEnumConstants`, `CppSemanticRecord.cpp:1181`). Si se publicara
  también el estado, habría dos fuentes para el mismo nombre en el módulo
  productor.
- No cambia ningún NSMETA existente válido (punto anterior), ni el formato, ni
  el catálogo. Tampoco cambia la semilla del catálogo: `declaredTypeKey` de estas
  constantes es `builtin:int`, que `provideCppCRecordMetadataCatalog` ignora.

### Medición con el parche aplicado de forma temporal

El parche se aplicó solo en el árbol de trabajo de este carril, se construyeron
`nsc` y `nsc_cpp_c_identity_tests` de forma incremental (hash `c756fa1bb11fb393`)
y se ejecutó `validar.cmd constantes` con `NSC_T08_PROPUESTAS=1`. Después se
revirtió con `git checkout`, se reconstruyó (hash `368fe14bbf0d356a`, fuente igual
a HEAD) y se volvió a ejecutar todo el validador con los resultados ACTUAL. No se
hizo ningún commit con el parche.

| Consumidor | ACTUAL | Con el parche A |
|---|---|---|
| `CConstNone`, `CConstSurfaceOnly` | ejecutan | ejecutan |
| `CConstNamedPlain` | productor: `state` | **ejecuta** |
| `CConstAnonPlain` | productor: `state` | **ejecuta** |
| `CConstNamedSurfaceValue` | productor: `state` | **ejecuta** |
| `CConstNamedSurfaceTemplate` | productor: `state` | **ejecuta**: la plantilla obtiene `CConstModeB` del catálogo |
| `CConstNamedTemplate` | productor: `state` | `unknown identifier 'CConstModeB'` en `PConstNamedTemplate.nsmeta.public-templates.ns:5:52` |
| `CConstAnonTemplate` | productor: `state` | `unknown identifier 'CConstLoose'` en `PConstAnonTemplate.nsmeta.public-templates.ns:5:48` |

Inspección del NSMETA con el parche: los siete productores construyen con
`states=0`. El catálogo de `PConstNamedSurface` y el de `PConstSurfaceOnly`
contienen `CConstMode: CConstModeA=10 CConstModeB=11`. Los demás no tienen
catálogo.

Conclusiones:

1. HECHO COMPROBADO: con el parche A no se publica ningún estado incompleto ni
   duplicado.
2. HECHO COMPROBADO: no se pierde ninguna constante que el consumidor pudiera
   obtener. Cuando el enum está en la superficie, la plantilla compila y ejecuta
   con la constante del catálogo.
3. HECHO COMPROBADO: dos casos siguen necesitando información adicional, y con el
   parche fallan en el consumidor, no en el productor. Es la misma situación que
   H3 para funciones y globales.

## Lo que falta para las plantillas (no incluido en el parche)

- **Enum con nombre fuera de la superficie** (`CConstNamedTemplate`), sin
  cambio de formato. En `provideCppCRecordMetadataCatalog`
  (`CppSemanticRecord.cpp:960`), añadir a `enumSeeds` el enum de cada constante C
  del proyecto raíz que use una plantilla pública. El enum se localiza por el
  USR del enumerador en el inventario de enums. Hay que decidir entre:
  - sembrar con todas las constantes importadas: más simple, pero publica
    también enums que solo usan cuerpos privados;
  - sembrar solo con las de plantillas: exige saber qué demandas vienen de la
    fuente de plantillas.

  No se preparó parche porque `CppSemanticRecord.cpp` es del carril de layout de
  records y la rama de Codex lo modifica para H4 A/B.
- **Enum anónimo** (`CConstAnonTemplate`): el catálogo no admite enums sin
  nombre. Requiere el cambio de formato de H3 o de la parte C de H4.

## Archivos que coordinar

| Archivo | Cambio | Compartido con |
|---|---|---|
| `src/Interop/LibraryMetadataConstruction.cpp` | parche A (una condición y su comentario) | metadata; carril de variables globales (publicación de estados) |
| `src/Interop/Cpp/CppSemanticRecord.cpp` | siembra del catálogo para plantillas (solo diseño) | layout de records; H4 A/B de Codex |
| `pruebas-tanda-08-identidad-c/constantes-enum/casos.txt` | al integrar el parche, la columna ACTUAL pasa a valer PROP | este carril |
| `pruebas-tanda-08-identidad-c/plantillas/casos.txt` | con el parche, `ConsumerEnumConstantPlain` y `ConsumerAnonymousConstantPlain` deberían ejecutar, y `ConsumerEnumConstant` y `ConsumerAnonymousConstant` dar `ident` (NO VERIFICADO: no se ejecutó esa sección con el parche) | este carril |
