# Tanda de Validación 15 — Clausura del esquema del manifiesto

## Alcance causal

La carga integral del manifiesto central estaba impedida por diferencias entre
el vocabulario ya materializado y el esquema del controlador. Se corrigió el
esquema y sus ejecutores; no se modificó ninguna fila del manifiesto.

## Correcciones

1. `short` y `medium` forman parte del contrato de timeout y tienen duraciones
   compartidas de 15 y 60 segundos, respectivamente.
2. La tabla de segundos vive en una sola fuente y la consumen tanto el ejecutor
   host como el ejecutor de procesos.
3. Los casos host admiten la capa `integration`, ya presente en el inventario.
4. Los aislamientos host reconocen `in-process-reset`, `temporary-directory` y
   `filesystem-temp`, los tres usados por casos canónicos.
5. El enum de clase cubre todas las clases existentes, incluidas variantes de
   precondición, estructura, escala, transacción y adversarial.
6. El grafo topológico sólo interpreta como aristas las dependencias con forma
   de ID de caso. Productores como LLVM, NSLink, CMake, bibliotecas o componentes
   self-host se conservan como requisitos externos y no se rechazan como casos
   ausentes.

## Evidencia dirigida

- `14/14` pruebas unitarias del esquema: PASS.
- carga estricta del manifiesto central: `5,695/5,695` filas.
- ninguna fila, oráculo o resultado esperado modificado.

## Límites preservados

- sin compilación ni ejecución de producto;
- sin baseline, `C:`, despliegue ni `push`;
- batería histórica y archivos protegidos intactos.
