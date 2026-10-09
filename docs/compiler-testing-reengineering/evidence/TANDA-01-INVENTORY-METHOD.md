# Evidencia de inventario — Tanda 01

## Clasificación

`HECHO COMPROBADO`: la revisión congelada es
`3bb175443a18b2049c20d75443dd6743e2993903`, observada en la rama local `development`.

`HECHO COMPROBADO`: existían seis archivos modificados ajenos a esta fase antes de crear sus documentos. Están
enumerados y protegidos en `SCOPE-LOCK.md`.

## Universo de producción y soporte

El manifiesto incluye archivos versionados bajo:

- `src/**` e `include/nativesharp/**`: compilador host C++;
- `selfhost/nsc/**`: compilador autocontenido NativeSharp;
- `selfhost/bootstrap/**` y `selfhost/contracts/**`: contratos productores y verificación self-host;
- `lib/**` y `sdk/**`: fuentes y metadata que el compilador distribuye o consume;
- `cmake/**` y `CMakeLists.txt`: composición, targets y gates;
- `tools/**`: herramientas versionadas relacionadas con inventario, build y verificación;
- `third_party/**`: dependencias vendorizadas;
- scripts raíz de build, deploy y empaquetado seleccionados expresamente.

La batería `tests/**` se conserva en un manifiesto independiente de congelación. No se utiliza como sustituto
del inventario contractual nuevo ni se modifica.

## Procedimiento reproducible

1. Enumerar con `git ls-files` únicamente rutas versionadas.
2. Ordenar rutas con comparación ordinal ignorando mayúsculas/minúsculas.
3. Asignar identificadores estables por orden: `PF-000001` y `HT-000001`.
4. Registrar tamaño, líneas y SHA-256 del contenido actual.
5. Clasificar rol, componente, lenguaje y disposición sin promover el estado documental inicial.
6. Exportar CSV y JSON UTF-8.
7. Calcular SHA-256 de cada manifiesto terminado.

La orden completa usada para generarlos queda registrada en la bitácora de la tanda y puede repetirse sin
compilar ni ejecutar pruebas.

## Limitación explícita

Los conteos y hashes sólo demuestran integridad del universo descubierto. No demuestran que un archivo haya
sido comprendido, clasificado semánticamente, consolidado, contrastado o auditado.

## Resultado de la enumeración

| Universo | Archivos | Líneas |
|---|---:|---:|
| Producción y soporte | 974 | 311,814 |
| Compilador primario host y self-host | 553 | 200,850 |
| Build y empaquetado | 79 | 11,825 |
| Contratos self-host | 122 | 9,056 |
| Dependencias distribuidas | 110 | 15,979 |
| Herramientas de soporte | 15 | 4,418 |
| Dependencias vendorizadas | 95 | 69,686 |
| Archivos históricos congelados | 1,658 | No aplica en este manifiesto |

El inventario de producción contiene 57 componentes documentales. El gate independiente encontró cero rutas
faltantes, extras o duplicadas, cero identificadores duplicados, cero clasificaciones desconocidas y cero
huellas distintas al árbol actual.

Los SHA-256 canónicos se conservan en `inventory/manifest-summary.json` y `CURSOR.json`; no se repiten aquí
para evitar que una copia manual se convierta en una segunda fuente de verdad.
