# Tanda 01.74A - Host.Language: interacción, corpus y performance

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-010` |
| Subtanda | 1 de 2 |
| Casos implementados | 10/18: `TL-LANG-124..133` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | EN CURSO |

Esta subtanda cubre las tres primeras superficies de calidad de integración: matrices de
interacción, corpus de programas completos y presupuestos de performance. La subtanda 74B
queda reservada para diagnósticos, evidencia semántica de debug y auditoría de costos.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-124` | Matriz mínima con los seis pares críticos y orden de casos alterno. |
| `TL-LANG-125` | Identidad vacía, menos de dos áreas, seed cero y duplicado con error exacto. |
| `TL-LANG-126` | Medium, sin minimización, sin impacto y sin triple con prioridad exacta. |
| `TL-LANG-127` | Siete IDs, áreas y seeds deterministas para cero, fallback explícito y `uint64_t` máximo. |
| `TL-LANG-128` | Minimización válida que sólo reemplaza identidad y áreas y conserva los demás campos. |
| `TL-LANG-129` | Menos de dos áreas, minimización desactivada y área ausente con `nullopt` exacto. |
| `TL-LANG-130` | Corpus válido y cada invariante base rota con el primer error exacto. |
| `TL-LANG-131` | Seis kinds, perfiles, unidades, exit 42 y cuatro perfiles partial-noheap. |
| `TL-LANG-132` | Comparación inclusiva de cinco métricas y exclusión expresa de backend/toolchain. |
| `TL-LANG-133` | Seis perfiles, tolerancia, samples, metadata y cinco violaciones ordenadas. |

## Propiedades adicionales verificadas

- La cobertura de pares críticos es independiente del orden de los casos.
- El seed base cero se sustituye por `0x4e533300`; el límite superior usa aritmética modular.
- La minimización conserva el orden solicitado de áreas esenciales.
- La validación extendida del corpus rechaza perfiles, kinds y cuota noheap incompletos.
- Backend y toolchain no participan en el presupuesto del frontend ni en sus violaciones.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido de la subtanda: Debug 10/10 en 4.88 s; Release 10/10 en 0.69 s.
- Suite nueva completa: Debug 70/70 en 284.64 s; Release 70/70 en 33.57 s.
- Controlador Python: 51/51 en 0.724 s.
- Fuentes Python de la suite: 91/91 compiladas en memoria.
- Manifiesto: 572 casos, 0 duplicados y 10 casos implementados de `CL-LANG-010`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en el módulo nuevo.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

Los casos `TL-LANG-124..133` quedan implementados y verificados sin bloqueos de producto.
`CL-LANG-010` permanece en curso hasta cerrar `TL-LANG-134..141` en la subtanda 74B.
