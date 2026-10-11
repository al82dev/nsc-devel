# Fase 2 — Superficie canónica de infraestructura futura

Fecha: 2026-10-10

## SCOPE LOCK

- Objetivo: separar del denominador runtime exactamente `TA-AST-017`, `TS-SEMA-077`,
  `TS-SEMA-094` y `TS-SRC-049`, conservando su identidad canónica y comprobando que no
  aparezcan en el manifiesto ni en registros ejecutables.
- Naturaleza: corrección de infraestructura y documentación.
- Archivos autorizados: catálogo futuro, gate y prueba estática del inventario, y este documento.
- Protegido: producto, CMake, Builder, cuerpos de prueba, manifiesto ejecutable y demás casos.
- Prohibido: compilar, ejecutar pruebas o habilitar perfiles sanitizados futuros.
- Terminación: el catálogo futuro es explícito y único; el gate valida diseño, ausencia del
  manifiesto y ausencia de solapes antes de comparar el resto del inventario runtime.

## Contrato

`tests/compiler-suite/manifest/future-infrastructure.jsonl` es la única superficie versionada
de estos cuatro casos. Cada fila conserva ID, contrato y componente del diseño, declara
`FutureInfrastructure` y nombra la macro del perfil futuro requerido.

El gate retira esas filas del denominador runtime sólo después de validar que pertenecen al
diseño y no están en `cases.jsonl`. Luego reserva sus IDs antes de combinar los registros
ordinario, destructivo, Source extendido, pending runtime y externo; cualquier aparición allí
se reporta como solape.

## Validación

Se añadió cobertura estática para el conjunto exacto y para aceptación, solape, omisión y
metadatos alterados. Por restricción explícita, no se compiló ni se ejecutó ninguna prueba.
