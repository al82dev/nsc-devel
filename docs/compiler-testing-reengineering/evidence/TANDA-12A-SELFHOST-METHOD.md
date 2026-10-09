# Evidencia de método — Tanda 12A, compilador self-host

## SCOPE LOCK aplicado

Auditoría estática exclusivamente documental de `selfhost/nsc/**`. No se modificó ni ejecutó producto, pruebas o infraestructura histórica.

## Universo

| Archivos | Fuentes `.ns` | Proyecto | README | Líneas | Bytes | Declaraciones | Contratos | Casos diseñados |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 29 | 27 | 1 | 1 | 12063 | 684862 | 874 | 138 | 1027 |

## Método reproducible

1. Se leyó completo cada archivo en orden de path y se contrastó el proyecto con el conjunto físico.
2. Un scanner iterativo conserva tipos y callables con ruta, línea, visibilidad y nombre calificado; no interpreta sentencias de control como declaraciones.
3. La lectura semántica separó contratos de valor, cursor, builder, grafo, proyecto, semántica, IR, ABI, runtime, proceso y emisión.
4. Cada declaración se asignó a un contrato primario. Los archivos no ejecutables trazan contratos de proyecto/documentación.
5. Los casos futuros se derivaron por contrato: válido mínimo, válido representativo, prerrequisito ausente, estructura inválida, bordes y variantes; determinismo, mutación y escala sólo cuando aplican.
6. Una segunda auditoría por componente recalculó archivos, declaraciones, contratos, casos y referencias.
7. Se regeneró la clasificación transversal de todos los contratos para incluir el self-host.

## Corrección de conteo preliminar

Los conteos exploratorios de 829/1456 y 496 no eran válidos: el primero admitía líneas de control y el segundo omitía accessors con cuerpo de expresión. El scanner persistido exige una declaración de tipo o un callable con visibilidad, incluye accessors observables y obtiene 874 declaraciones físicas (82 tipos y 792 callables). El conteo corregido es el único promovido.

## Límites

No hubo build, ejecución, baseline, cobertura, instalación, despliegue, modificación de C: ni acceso remoto. `RECONSTRUIBLE` significa suficiencia documental para esta revisión, no suite implementada.
