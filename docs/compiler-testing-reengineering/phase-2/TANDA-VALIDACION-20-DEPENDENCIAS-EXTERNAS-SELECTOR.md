# Tanda de Validación 20 — Dependencias externas del selector

## Defecto reproducido

El manifiesto aceptaba productores externos en `dependencies`, pero
`select_cases` intentaba resolver cada cadena como un caso. La selección real
de `T12B-00001` abortaba en `CMake` antes de alcanzar ejecución.

## Causa

El cargador y el selector aplicaban reglas distintas al mismo campo. El grafo
del cargador sólo incluía valores con forma de ID de caso; el selector recorría
indistintamente IDs y productores externos.

## Corrección

La clausura transitiva filtra aristas mediante la expresión canónica `CASE_ID`.
Los nombres externos permanecen inalterados en `CaseManifest.dependencies`; un
valor con forma de ID de caso continúa obligado a existir y se incorpora a la
selección.

## Evidencia dirigida

- selector: `9/9` pruebas Python PASS;
- `T12B-00001`: selección PASS con `CMake`, `LLVM 23` y `NSLink` preservados;
- ID ausente `TB-BAS-999`: rechazo PASS;
- selección completa Debug: `5,695/5,695` filas.

## Límites preservados

- sin modificación del manifiesto ni de dependencias declaradas;
- sin ejecución de producto;
- sin configuración, compilación, baseline, `C:`, despliegue ni `push`;
- batería histórica y seis archivos protegidos intactos.
