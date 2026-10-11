# Pendientes de testing y capacidades — Fase 2

Fecha: 2026-10-09

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Producto | Documentar; no corregir el compilador en esta tanda |
| Suite nueva | Conservar los IDs y oráculos; no simular éxito |
| Suite histórica | Intacta; no ejecutar |
| Componentes externos | Excluidos: bibliotecas, SDK, bindings y otros repositorios |
| Infraestructura prohibida ahora | Baseline, cobertura, despliegue y `C:` |

## Expectativas retiradas del plan activo

La suite no define el producto futuro. Dos expectativas fueron retiradas porque
no forman parte de la superficie vigente:

| Caso | Expectativa inventada | Caso corregido contra el producto real |
|---|---|---|
| `TL-LANG-034` | Offsets individuales que `ClosureLayout` no expone. | Tamaño y alineación públicos para closure vacío y captures de alineación mixta. |
| `TL-LANG-201` | Operación remainder ausente de `IntegerArithmeticOperation`. | División, división por cero y `signed min / -1` bajo las cuatro políticas vigentes. |

Los IDs se conservan porque ambos tienen capacidad real que probar. No se añade
una feature pendiente, no se amplía el compilador y no se reduce el denominador.

## Infraestructura especial pendiente

### Trabajo futuro fuera del pipeline ordinario

Esta depuración conserva cuatro cuerpos de prueba como trabajo futuro, pero no
los publica en el manifiesto ni los registra como ejecutables del pipeline
ordinario:

| Caso | Perfil futuro requerido | Estado actual |
|---|---|---|
| `TA-AST-017` | target test-only con ThreadSanitizer | fuera del pipeline; no bloquea ni cuenta como ejecutable |
| `TS-SEMA-077` | target test-only con ASan+LSan | fuera del pipeline; no bloquea ni cuenta como ejecutable |
| `TS-SEMA-094` | target test-only con ASan+LSan | fuera del pipeline; no bloquea ni cuenta como ejecutable |
| `TS-SRC-049` | target ASan aislado en subprocess | fuera del pipeline; no bloquea ni cuenta como ejecutable |

Sus IDs, cuerpos y oráculos permanecen trazados para una integración futura con
los perfiles instrumentados correspondientes. Su ausencia del pipeline no se
interpreta como `PASS`, `SKIP` ni validación funcional.

### `TS-SRC-049` — invalidación de referencia por realocación

El caso conserva una referencia a un elemento de `std::vector<SourceFile>` y la
usa después de una realocación. Esa desreferencia es comportamiento indefinido;
comparar direcciones sin usar el objeto no demuestra el defecto.

- Infraestructura requerida: target separado instrumentado con AddressSanitizer
  y ejecución en subprocess.
- Oráculo: diagnóstico de use-after-free/use-after-reallocation y terminación
  controlada del proceso hijo; el proceso principal de la suite no debe quedar
  contaminado.
- Aislamiento: este perfil no forma parte del binario ordinario `/MT` Debug o
  Release y no debe introducir el runtime del sanitizer en esos artefactos.
- Estado: `ASAN PENDIENTE`; se requiere definir y validar el perfil versionado de
  build antes de activar el caso.

## Defectos de producto con prueba conservada

Los siguientes casos pertenecen a la suite y deben materializarse con el oráculo
correcto aunque inicialmente fallen:

| Caso | Defecto que debe revelar |
|---|---|
| `TL-LANG-019` | Overflow en layout de agregado. |
| `TL-LANG-035` | Overflow en layout de closure. |
| `TL-LANG-099` | Enum externo fuera de rango en expresión. |
| `TL-LANG-121` | Dominancia asimétrica ante duplicados del fallback. |
| `TL-LANG-230` | Escape numérico parcial y salida parcial tras error. |
| `TL-LANG-250` | Enum externo fuera de rango en smart pointer. |
| `TL-LANG-262` | Overflow en alineación final de variant. |
| `TL-LEX-075` | Seis comillas se consumen como un delimitador raw de ancho 6 en vez de apertura y cierre de ancho 3. |

Los ocho casos ya están materializados con estado
`PRUEBA IMPLEMENTADA — DEFECTO ABIERTO`. Se ejecutan individualmente y reportan
`KNOWN_FAILURE`; no forman parte del flujo ordinario hasta que una tarea de
producto corrija la causa. Está prohibido marcarlos `SKIP`, invertir la
expectativa o aceptar el comportamiento defectuoso.

## Trabajo futuro de Host.AST

| Caso | Contrato | Infraestructura necesaria | Motivo |
|---|---|---|---|
| `TA-AST-017` | `CA-AST-002` | Target instrumentado con TSAN | La ausencia de sincronización sólo puede diagnosticarse fielmente bajo carrera real instrumentada. |

`TA-AST-014`, `TA-AST-016`, `TA-AST-215`, `TA-AST-223` y `TA-AST-224` ya son
casos `KnownProductDefect` NIGHTLY del runner destructivo aislado. Sólo
`TA-AST-017` permanece pendiente y fuera del pipeline; no bloquea `Host.Parser`
ni otros contratos causalmente independientes.

## Host.Parser materializado y activo

Los `536` casos de Parser ya existen en la suite nueva. Diecinueve conservan un
defecto de producto mediante `KNOWN_FAILURE` y no bloquean el flujo ordinario:

`TP-PAR-063`, `TP-PAR-069`, `TP-PAR-103`, `TP-PAR-306`,
`TP-PAR-309`, `TP-PAR-316`, `TP-PAR-327`, `TP-PAR-334`, `TP-PAR-355`,
`TP-PAR-356`, `TP-PAR-358`, `TP-PAR-400`, `TP-PAR-405`, `TP-PAR-406`,
`TP-PAR-422`, `TP-PAR-431`, `TP-PAR-450`, `TP-PAR-476` y `TP-PAR-511`.

`TP-PAR-514` y `TP-PAR-524` están activos como `KnownProductDefect`;
`TP-PAR-516` está activo como `Required`. Los tres salieron del catálogo
`InfrastructurePending`, aunque esta transición documental no los presenta como
verificados.

## Host.Sema materializado con estado pendiente

Los `321` casos `TS-SEMA-001..321` están materializados. Cinco defectos de
producto permanecen en el manifiesto como `KNOWN_FAILURE`:

| Caso | Defecto que debe revelar |
|---|---|
| `TS-SEMA-106` | Un field de tipo función no reporta `InvalidType`. |
| `TS-SEMA-110` | La recursión nominal indirecta no se rechaza por incompletitud. |
| `TS-SEMA-246` | `validateObjectType` acepta `Function`. |
| `TS-SEMA-265` | Un struct C ABI autorreferente no termina por falta de visited-set. |
| `TS-SEMA-317` | `SymbolDumper` no emite el payload constante presente. |

Cinco casos permanecen fuera del manifiesto ejecutable ordinario hasta disponer
de la infraestructura fiel:

| Caso | Infraestructura necesaria |
|---|---|
| `TS-SEMA-077` | Perfil sanitizer para certificar ownership del grafo internado. |
| `TS-SEMA-094` | Perfil sanitizer para fugas y doble liberación del analyzer. |
| `TS-SEMA-306` | Proceso aislado para profundidad no acotada de fallthrough. |
| `TS-SEMA-314` | Proceso aislado para grafos profundos/cíclicos del SemanticDumper. |
| `TS-SEMA-321` | Proceso aislado para ciclos del grafo de miembros de SymbolDumper. |

`TS-SEMA-101` y `TS-SEMA-281` están activos como `Required`;
`TS-SEMA-265` está activo como `KnownProductDefect`. Ninguno se presenta como
verificado en esta reconciliación documental.

`TS-SEMA-089` no es una feature pendiente: el plan fue corregido de seis a
cinco clases globales soportadas, conforme al contrato vigente. La clase se
rechaza explícitamente en `TS-SEMA-090`.

## Decisión de avance independiente

`TS-SRC-049` permanece pendiente y visible; no se elimina ni permite declarar
lista la ola 1. Como no es productor de `Host.Lexer`, `Host.AST`, `Host.Parser`
ni `Host.Sema`, tampoco bloquea causalmente la ola 2. `TS-SRC-008`,
`TS-SRC-040` y `TS-SRC-041` ya salieron de esta lista al integrarse en los
perfiles WEEKLY/NIGHTLY, sin presentarse todavía como validados.

`TA-AST-017`, `TS-SEMA-077` y `TS-SEMA-094` siguen la misma política de trabajo
futuro: permanecen fuera del pipeline ordinario y no bloquean su cierre.

## Corrección del caso mal definido

`TL-LANG-116` era un defecto del test plan. El productor crea:

```text
$nsc_const$<typeLength>:<type>:<normalizedValue>
```

El parser asigna a `normalizedValue` todos los bytes posteriores al segundo
separador. No existe longitud, terminador ni gramática externa que permita
distinguir un supuesto `trailing` del valor. La prueba corregida conserva los
rechazos verificables de prefijo, decimal, overflow y separador, y confirma que
un marker válido consume el sufijo completo. No cambia el compilador.

## Efecto sobre la secuencia de trabajo

Los cuatro pendientes de infraestructura permanecen visibles y los ocho
defectos no se ocultan. `TL-LANG-034`, `TL-LANG-116` y `TL-LANG-201` se corrigen
como casos del plan contra capacidades reales. La Fase 2 puede continuar por
contratos y olas independientes, pero no puede declarar completa la ola 1 ni
verificar los casos rojos o pendientes hasta que sus condiciones reales se
resuelvan.
