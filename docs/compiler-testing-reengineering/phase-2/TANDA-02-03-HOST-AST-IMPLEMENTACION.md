# Tanda 02.03 — Implementación de Host.AST

Fecha: 2026-10-09

## Propósito

Materializar la suite nueva de `Host.AST` definida por la Fase 1 y conectarla al ejecutor consolidado de
componentes. Esta tanda implementa los casos que pueden ejecutarse fielmente con la infraestructura ordinaria
actual y conserva visibles, sin reducir el denominador, los casos que necesitan aislamiento especial.

## SCOPE LOCK

- Repositorio autorizado: `A:\nsc-devel`.
- Naturaleza: infraestructura y pruebas nuevas de Fase 2; no cambia el producto.
- Evidencia histórica protegida: las 684 pruebas y su infraestructura permanecen intactas.
- Archivos preexistentes protegidos: los seis registrados en `phase-2/CURSOR.json` no se editan ni se incluyen.
- Producto protegido: no se modifica `src/AST`, `include/nativesharp/AST` ni otro código del compilador para hacer
  pasar una prueba.
- Acciones prohibidas: tocar `C:`, desplegar, usar remotos, ejecutar baseline o implementar cobertura.
- Criterio de esta tanda: 224 casos ejecutables registrados con trazabilidad exacta y seis limitaciones especiales
  visibles; la verificación integrada se documenta por separado.

## Inventario cerrado

La fuente canónica contiene `TA-AST-001..230`. La distribución implementada es:

| Grupo | Contratos | Diseñados | Implementados |
|---|---|---:|---:|
| Arena y `AstNodeKind` | `CA-AST-001..003` | 26 | 23 |
| Registros y estado de `AstNode` | `CA-AST-004..010` | 75 | 75 |
| `TypeSyntax` | `CA-AST-011..017` | 76 | 76 |
| `AstDumper` | `CA-AST-018..020` | 53 | 50 |
| Total | `CA-AST-001..020` | 230 | 224 |

Los seis casos no materializados son `TA-AST-014`, `TA-AST-016`, `TA-AST-017`, `TA-AST-215`,
`TA-AST-223` y `TA-AST-224`. No desaparecen del inventario ni cuentan como implementados.

## Implementación

Los casos se distribuyen en 19 módulos cohesionados. Cada módulo prueba directamente las clases reales del
producto y registra `case_id`, `contract_id`, componente, capa, perfil y tags en `TestRegistry`. No se creó una
réplica de AST ni un sustituto del compilador.

- `AstArenaModule.cpp` prueba construcción, alineación, estabilidad y destrucción.
- Los módulos `AstNode*` y `AstRecordsModule.cpp` cubren identidad, tipos, genéricos, lifetime, topología y estado.
- Los módulos `TypeSyntax*` cubren identidad, ownership, argumentos, funciones, constantes, modificadores y origen.
- Los módulos `AstDumper*` verifican bytes, tipos, nodos, aristas y recorridos finitos.

`TypeSyntaxTestSupport.h` contiene únicamente fixtures y registro común de casos unitarios. La integración agrega
declaraciones y registros al ejecutor existente, las cinco unidades de producto AST necesarias al target y un gate
controlador exclusivo de `Host.AST`.

## Manifiesto y gate

`manifest/cases.jsonl` incorpora 224 filas ordenadas. Cada fila conserva los IDs, contrato, capa y frecuencia del
diseño de Fase 1. El gate `run_host_ast_gate.py` reconstruye el conjunto esperado como `001..230` menos las seis
limitaciones explícitas, exige orden e identidad exactos y rechaza cualquier resultado distinto de `PASS`.

## Algoritmos, ownership y complejidad

Las pruebas usan fixtures de ownership local. Las relaciones entre nodos son no propietarias y no sobreviven al
scope de sus propietarios. Los recorridos de verificación son iterativos salvo que invoquen la API productiva que
se está caracterizando; la infraestructura nueva no introduce recursión. El costo ordinario es lineal respecto del
número de nodos o registros construidos por cada caso.

## Evidencia previa al commit

- Inventario estático: `224` IDs únicos materializados.
- Ausencias exactas: las seis limitaciones enumeradas, sin otras omisiones.
- Pruebas dirigidas de los módulos: `224/224 PASS` informadas por los tres bloques de implementación.
- Validación integrada Release/Debug: pendiente del commit de implementación y se registrará en la tanda siguiente.

## Análisis de austeridad y diferencial

El diferencial sólo agrega módulos de prueba, manifiesto, gate e integración mínima. No incorpora wrappers de
producto, fallbacks, cambios semánticos, excepciones nuevas en el compilador ni limpiezas adyacentes. La prueba
negativa o costosa que no puede expresarse fielmente permanece como limitación, no se reemplaza por un proxy.

## Pendientes

- Ejecutar el target consolidado y el gate exclusivo de `Host.AST` en Release y Debug.
- Implementar las seis pruebas especiales cuando exista su infraestructura determinista aprobada.
- Continuar `Host.Parser` como trabajo independiente de la misma ola.
