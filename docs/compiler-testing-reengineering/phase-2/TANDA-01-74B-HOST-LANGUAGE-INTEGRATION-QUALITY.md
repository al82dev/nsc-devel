# Tanda 01.74B - Host.Language: diagnósticos, debug y costos

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-010` |
| Subtanda | 2 de 2 |
| Casos implementados en esta subtanda | 8: `TL-LANG-134..141` |
| Contrato acumulado | 18/18: `TL-LANG-124..141` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

Esta subtanda cierra calidad y catálogo de diagnósticos, renderizado terminal, identidad y
evidencia semántica de debug, costos ocultos, auditoría zero-cost y versión del esquema de
tooling. Todos los oráculos comparan decisiones, estados o bytes públicos exactos.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-134` | Código, idiomas, span, fix-it, cascades y backtrace válidos e inválidos. |
| `TL-LANG-135` | Cinco áreas, width mínimo, span físico, códigos duplicados y área ausente. |
| `TL-LANG-136` | Inglés/español y widths 0, 1, 3, 12 y completo con truncamiento exacto. |
| `TL-LANG-137` | Owner, role y ordinal con vacíos, alfanuméricos y puntuación. |
| `TL-LANG-138` | Ocho kinds, O0/O2, identidad, scopes, points, inline, generated y valueInvented. |
| `TL-LANG-139` | Conjunto vacío, costos explícitos y primer costo oculto. |
| `TL-LANG-140` | Seis costos, dos modos, MIR/assembly, excepciones, duplicados y cobertura. |
| `TL-LANG-141` | Bytes `nsc-semantic-tooling-v1` y almacenamiento estable entre llamadas. |

## Propiedades adicionales verificadas

- La validación diagnóstica preserva la prioridad entre campos generales, span, fix-it y backtrace.
- Widths de cero a tres truncan por bytes sin elipsis; los mayores reservan tres bytes para `...`.
- Una identidad de debug es única por nombre estable y nivel de optimización.
- O0 exige sequence points y prohíbe `OptimizedOut`; O2 nunca puede inventar valores.
- Las excepciones de costo sólo son válidas para costos no solicitados explícitamente.
- La cobertura zero-cost exige los seis kinds y al menos un registro Off y uno On.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido final: Debug 18/18 en 8.94 s; Release 18/18 en 1.01 s.
- Suite nueva completa final: Debug 70/70 en 293.47 s; Release 70/70 en 34.97 s.
- Controlador Python: 51/51 en 0.679 s.
- Fuentes Python de la suite: 91/91 compiladas en memoria.
- Manifiesto: 580 casos, 0 duplicados y 18 casos implementados de `CL-LANG-010`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en el módulo completo.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-010` queda completo con 18/18 casos implementados y verificados y sin bloqueos de
producto. La continuación de Ola 1 debe tomar `CL-LANG-011`.
