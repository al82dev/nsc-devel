# Evidencia de método — Tanda 12B, superficies restantes

## SCOPE LOCK aplicado

Auditoría estática y documental de build, empaquetado, bootstrap auxiliar, contratos self-host, bibliotecas distribuidas, tooling y terceros. No se modificó ni ejecutó producto, pruebas o infraestructura histórica.

## Universo

- Archivos: 421.
- Líneas: 110964.
- Componentes documentales: 17.
- Declaraciones/artefactos: 2233.
- Contratos directos/indirectos: 301.
- Casos diseñados: 1806.
- Disposiciones: DIRECTA=8, INDIRECTA=293, UPSTREAM=95, EXCLUIDA=25.

## Método reproducible

1. Se recorrió iterativamente el universo canónico por path y se contrastó cada hash.
2. Se leyeron targets, variables, procesos, funciones, declarations NativeSharp, schemas, manifests, imports, procedencia y licencias.
3. Cada archivo recibió exactamente una disposición; upstream y exclusiones no generan cobertura ficticia.
4. Los stubs intrínsecos se asignaron a lowering/ejecución, no a su cuerpo fuente.
5. Los adaptadores históricos se conservaron como gates indirectos y no como nuevas unitarias.
6. Una segunda pasada recalculó filas, símbolos, contratos, casos y referencias.

## Límites

No hubo build, ejecución, baseline, coverage, instalación, deploy, escritura en C: ni acceso remoto. Los casos están diseñados, no implementados.