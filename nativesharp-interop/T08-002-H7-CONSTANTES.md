# T08-002: constantes de enum C en bibliotecas (hallazgo H7)

Estado: **REPRODUCIDO**. Ninguna corrección aplicada en la rama.

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

HECHO COMPROBADO (validador, sección `constantes`):

- Fallan en el productor los cinco proyectos que usan una constante, con
  nombre o anónima, en método ordinario o en plantilla. Los dos controles
  construyen.
- `CConstSurfaceOnly` ejecuta: el consumidor usa `CConstModeB` restaurado del
  catálogo del productor.
- El modo `--inspect-c-constants` de `nsc_cpp_c_identity_tests` lee el NSMETA de
  cada productor que se construyó en esa ejecución. Comprueba que no hay estados
  sin espacio de nombres, que no hay estados con el nombre de la constante y que
  el catálogo contiene el enumerador requerido. Los controles pasan con 0
  estados publicados.

## Qué casos necesitan información adicional en el consumidor

Por lectura del código, confirmado después con la corrección aplicada de forma
temporal (ver la sección de corrección):

- **Método ordinario** (con nombre y anónimo): el valor queda plegado en el
  cuerpo compilado. El consumidor no necesita nada más.
- **Plantilla con el enum en la superficie** (`PConstNamedSurface`): el catálogo
  ya lleva el enum y sus enumeradores. `reconstructCEnumConstants` los restaura
  en el ámbito del productor, donde se vuelve a analizar la plantilla. No hace
  falta nada más.
- **Plantilla con un enum con nombre fuera de la superficie**
  (`PConstNamedTemplate`): el catálogo no contiene el enum. Hace falta añadirlo
  al catálogo, sin cambio de formato porque el catálogo ya transporta enums con
  nombre.
- **Plantilla con un enum anónimo** (`PConstAnonTemplate`): el catálogo solo
  admite enums con nombre (`CppCRecordMetadataCodec.cpp:500`). Hace falta
  información nueva (H3 o la parte C de H4), que es un cambio de formato.
