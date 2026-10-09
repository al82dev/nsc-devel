# Tanda 01.76 - Host.Language: superficie histórica

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-012` |
| Casos implementados | 7: `TL-LANG-155..161` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

La tanda caracteriza el catálogo histórico vigente de 16 features, sus lookups, fallback,
manifiesto y diagnósticos. Este cierre congela el comportamiento observable del módulo actual;
no convierte las etiquetas históricas `3.0`/`4.0` en una decisión nueva de diseño del lenguaje.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-155` | Dieciséis entries con feature, nombre, disposition, rationale y orden exactos. |
| `TL-LANG-156` | Cada enum válido retorna una referencia a su entry correspondiente. |
| `TL-LANG-157` | Valores externos -1, 16, 17 e `int` máximo usan el fallback vigente a la primera entry. |
| `TL-LANG-158` | Bytes completos del manifest, headers, labels, orden y newline terminal. |
| `TL-LANG-159` | Diagnóstico exacto para las seis features `Implemented`. |
| `TL-LANG-160` | Diagnóstico exacto para nueve `DeferredTo4` y una `RejectedFor3`. |
| `TL-LANG-161` | Repetición determinista sin mutación de tabla, manifest ni diagnostics. |

## Propiedades adicionales verificadas

- El catálogo y los lookups comparten las mismas referencias estáticas.
- El fallback de enum externo queda caracterizado, no inferido ni reinterpretado.
- El manifest se compara completo, no mediante substrings, hashes ni conteos rígidos aislados.
- Todos los diagnostics se comparan por bytes completos, incluido cada rationale.
- Las llamadas a diagnostics no alteran una lectura posterior del manifest.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 7/7 en 3.82 s; Release 7/7 en 0.54 s.
- Suite nueva completa: Debug 72/72 en 320.51 s; Release 72/72 en 35.35 s.
- Controlador Python: 51/51 en 0.665 s.
- Fuentes Python de la suite: 93/93 compiladas en memoria.
- Manifiesto: 600 casos, 0 duplicados y 7 casos de `CL-LANG-012` en orden canónico.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; C++ y Python añadidos con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-012` queda completo con 7/7 casos implementados y verificados, sin bloqueos de
producto. La continuación de Ola 1 debe tomar `CL-LANG-013`.
