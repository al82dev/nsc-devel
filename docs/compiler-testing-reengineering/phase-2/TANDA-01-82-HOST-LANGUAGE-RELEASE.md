# Tanda 01.82 - Host.Language: freeze, conformance y promoción

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-018` |
| Casos implementados | 18/18: `TL-LANG-231..248` |
| Casos bloqueados | Ninguno |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

La tanda comprueba los manifests de freeze, conformance y especificación bilingüe,
las políticas de release candidate, la promoción ordinaria y oficial, las identidades
estables y la ausencia de estado global entre llamadas repetidas.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TL-LANG-231..234` | Revisiones, hashes, targets y gates de freeze completos o ausentes. |
| `TL-LANG-235..236` | Identidades únicas, tier Mandatory, tres tiers y cuatro evidencias. |
| `TL-LANG-237..238` | Paridad bilingüe, notes, glossary, links, samples e indexes. |
| `TL-LANG-239..241` | Matriz 3x3 de cambios y reinicio obligatorio de gates afectados. |
| `TL-LANG-242` | Las 512 combinaciones de nueve condiciones esenciales de promoción. |
| `TL-LANG-243..245` | Evidencia faltante individual y conjunta en orden contractual estable. |
| `TL-LANG-246..248` | Identidades exactas, determinismo y política histórica vigente. |

## Propiedades adicionales verificadas

- Un manifest mínimo de conformance requiere al menos un caso Mandatory no vacío.
- La ausencia de una versión del candidate conserva prioridad sobre sus tiers y evidencias.
- Un clause set bilingüe vacío falla sin inventar un diagnóstico textual.
- El error bilingüe respeta la prioridad parity, notes, glossary y gates finales.
- RC1 y RC2 rechazan features; RC3 acepta únicamente promotion blockers.
- Cualquier cambio aceptado exige reiniciar los gates afectados.
- `officialPromotionStatus` reemplaza el vector de faltantes en cada llamada.
- Las llamadas valid/invalid/valid repetidas 64 veces no conservan estado global.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 18/18 en 11.51 s; Release 18/18 en 1.06 s.
- Suite nueva completa: Debug 78/78 en 452.24 s; Release 78/78 en 43.97 s.
- Controlador Python: 51/51 en 0.703 s.
- Fuentes Python de la suite: 99/99 parseadas en memoria.
- Manifiesto: 685 casos, 0 duplicados y 18 casos implementados de `CL-LANG-018`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en los tres módulos nuevos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-018` queda completo con 18 casos implementados y verificados. La Fase 2 puede
continuar con `CL-LANG-019` sin pendientes propios de este contrato.
