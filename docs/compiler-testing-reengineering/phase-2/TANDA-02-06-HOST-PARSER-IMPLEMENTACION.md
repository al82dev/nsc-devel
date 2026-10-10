# Tanda 02.06 — Implementación de Host.Parser

Fecha: 2026-10-09

## Alcance

Se materializaron los `536` casos `TP-PAR-001..536` de los contratos
`CP-PAR-001..042`. El cambio pertenece exclusivamente a la suite nueva:

- no modifica `Parser.cpp`, `Parser.h` ni otro código de producto;
- no toca la batería histórica;
- no usa `C:`, despliegue, baseline ni remotos;
- no ejecuta compilaciones, pruebas ni gates en esta etapa, por instrucción
  expresa de completar primero toda la implementación de la Fase 2.

## Organización implementada

Los casos quedaron distribuidos por responsabilidad y rango estable:

| Rango | Responsabilidad |
|---|---|
| `001..109` | cursor, recuperación, lookahead, tipos y genéricos |
| `110..200` | imports, namespaces y declaraciones nominales |
| `201..300` | miembros, operadores, callables y bloques |
| `301..400` | statements, asm, control, patterns, match, loops y foreach |
| `401..500` | cleanup y expresiones |
| `501..536` | interpolación, construcción, clonado, estado y robustez |

La implementación usa un ejecutable host consolidado, no un ejecutable por
caso. `ParserProductTestShim.cpp` compila el cuerpo real de `Parser.cpp` dentro
del target de pruebas después de habilitar el seam privado exclusivo de tests.
Esto mantiene iguales las declaraciones y definiciones bajo ABI de Windows sin
alterar la superficie del compilador.

## Integración materializada

- `45` módulos `Parser*Module.cpp` registran los `536` IDs.
- `5` headers `Parser*TestSupport.h` contienen fixtures y helpers iterativos.
- `ParserProductTestShim.cpp` es el único puente con la implementación real.
- `TestRegistry.h` y `main.cpp` registran todos los módulos.
- `CMakeLists.txt` incorpora módulos, shim y gate controlador.
- `cases.jsonl` contiene `533` casos ejecutables.
- `run_host_parser_gate.py` conserva el conjunto exacto y sus estados causales.

Tres casos quedan materializados en el registro host pero fuera del manifiesto
ejecutable ordinario porque necesitan una seam todavía inexistente:
`TP-PAR-514`, `TP-PAR-516` y `TP-PAR-524`.

## Corrección de asignación

`TP-PAR-525..536` inspeccionan estado interno, ownership prestado, contadores,
restauración de contexto y progreso del Parser. Ninguno de esos oráculos es
observable mediante la CLI pública de `nsc.exe`. Su asignación inicial a
`nsc_suite_controller+nsc` era incompatible con el propio contrato; se corrigió
a `nsc_component_tests`, `RESET_PER_CASE` y optimización `N/A`. No se cambió el
oráculo ni el producto.

## Estados visibles

El manifiesto mantiene `20` casos con `expected_outcome=KNOWN_FAILURE`; el
ejecutor ordinario no los confunde con PASS y el gate exige exactamente su
estado causal. Los tres casos de infraestructura especial permanecen fuera del
flujo ordinario y en el registro de pendientes.

## Estado de validación

```text
Implementación: MATERIALIZADA
Integración declarativa: MATERIALIZADA
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Motivo: validación offline diferida hasta completar toda la Fase 2
```

Por lo anterior, los `536` casos cuentan como implementados, pero ninguno añade
todavía un resultado verificado al tracking.
