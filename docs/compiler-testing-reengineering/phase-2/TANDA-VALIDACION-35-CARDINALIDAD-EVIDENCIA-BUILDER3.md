# Tanda de Validación 35: cardinalidad terminal de la evidencia

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados

La evidencia aceptaba tres cierres inconsistentes:

1. menos resultados que el `variant_count` anunciado;
2. dos resultados con la misma clave terminal;
3. una cardinalidad declarada con tipo inválido.

Los tres podían producir archivos finales con apariencia de cierre.

## Corrección mínima

`EvidenceStore` conserva la cardinalidad declarada y, antes de cualquier
artefacto terminal:

- rechaza claves duplicadas;
- valida que `variant_count` sea un entero positivo;
- exige igualdad entre variantes previstas y resultados recibidos.

Si falla el gate, los eventos y el log ya vaciados permanecen como evidencia
progresiva, pero no existen `results.json`, `summary.md` ni `RUN_FINISHED`.

## Evidencia

- Omisión reproducida y rechazada: `1`.
- Duplicado reproducido y rechazado: `1`.
- Cardinalidad inválida reproducida y rechazada: `1`.
- Corrida sintética completa: `2/2` claves, `2/2` pares de eventos.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `103/103` PASS.

La prueba escribió únicamente en un temporal bajo
`A:\nsc-devel\_nscbuilder3` y lo retiró al terminar. No creó procesos ni ejecutó
producto; no hubo baseline, despliegue ni escritura en `C:`.
