# Validacion 135 - particion exacta del inventario global

## Alcance

Esta tanda corrige el supuesto equivocado del gate creado en Validacion 26.
No elimina casos y no relaja la comprobación a un subconjunto: exige igualdad
exacta del denominador completo mediante sus tres productores reales.

## Particiones canónicas

| Productor | Casos | Ejecución |
|---|---:|---|
| `TestRegistry` mediante `--list` | 8,054 | registro C++ ordinario |
| colecciones mediante `--list-pending` | 841 | no ejecutable; InfrastructurePending |
| `nsc_suite_controller+nsc` | 221 | proyecto/artefacto NativeSharp externo |
| **Total** | **9,116** | denominador Host/SelfHost/Infrastructure |

Los 221 externos se derivan sin rangos privados: 196 pertenecen al manifiesto
vivo con ejecutor `nsc_suite_controller+nsc`; otros 25 son clasificaciones no
ejecutables de esos mismos componentes, leídas desde los JSONL estrictos.

## Reglas del gate

El gate ahora:

1. carga las 9,116 identidades canónicas desde CSV/JSONL;
2. ejecuta `--list` y `--list-pending`;
3. deriva el productor externo desde el manifiesto y clasificaciones vivas;
4. rechaza cualquier ID presente en más de una partición;
5. rechaza omisiones, inesperados y cambios de contrato o componente;
6. exige que la unión sea exactamente el inventario canónico.

No usa una lista privada de IDs ni congela cardinalidades como criterio de
aceptación. Los conteos de esta tanda son evidencia del estado observado.

## Validacion

- pruebas Python dirigidas: `4/4` PASS;
- `nsc.compiler-suite.registry-inventory`: `1/1` PASS;
- duración del gate: `18.62 s`;
- solapamientos: `0`;
- omisiones: `0`;
- inesperados: `0`;
- cambios de contrato/componente: `0`.

No se ejecutó baseline, no se desplegó y no se tocó `C:`. La tarea persistente
continúa con el barrido completo de gates Debug.
