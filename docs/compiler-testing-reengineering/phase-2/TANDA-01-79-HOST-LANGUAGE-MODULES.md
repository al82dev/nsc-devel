# Tanda 01.79 - Host.Language: módulos

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-015` |
| Casos implementados | 10: `TL-LANG-182..191` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

La tanda caracteriza identidad de módulos Windows, resolución de grafos, imports, visibilidad,
reexports, availability y la entrada canónica del fingerprint de API pública. El caso profundo
conserva frecuencia NIGHTLY y las demás pruebas permanecen en PR.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-182` | Normalización exacta de case, slash y package/module vacíos. |
| `TL-LANG-183` | Grafo vacío, DAG lineal y diamante resuelven sin error. |
| `TL-LANG-184` | Ciclos Layout fallan con diagnóstico exacto; ciclos Binding son válidos. |
| `TL-LANG-185` | El resolvedor recursivo vigente acepta una cadena Layout acíclica de 1,024 nodos. |
| `TL-LANG-186` | Cinco tipos de import se deduplican y ordenan por alias, identity y kind. |
| `TL-LANG-187` | Target vacío, alias inválido y colisión case-insensitive fijan error y resultado. |
| `TL-LANG-188` | Matriz completa: cuatro visibilities por 16 combinaciones de relaciones. |
| `TL-LANG-189` | Un reexport conserva original identity con facade nombrada o vacía. |
| `TL-LANG-190` | Architecture mismatch precede deprecated; architecture vacía acepta cualquier target. |
| `TL-LANG-191` | Sólo símbolos públicos, ordenados por identity, forman los bytes exactos del fingerprint. |

## Propiedades adicionales verificadas

- La prueba nueva construye la cadena profunda de forma iterativa; no introduce recursión.
- Un error de import temprano limpia el resultado; una colisión tardía conserva el resultado ordenado.
- La matriz de acceso recorre 64 combinaciones y calcula el oráculo desde el contrato de cada visibility.
- Los símbolos privados y el orden de declaración no cambian el fingerprint público.
- El perfil NIGHTLY acumula los nueve casos PR y el caso profundo sin omisiones.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido NIGHTLY: Debug 10/10 en 5.85 s; Release 10/10 en 0.66 s.
- Suite nueva completa: Debug 75/75 en 361.12 s; Release 75/75 en 38.19 s.
- Controlador Python: 51/51 en 0.771 s.
- Fuentes Python de la suite: 96/96 compiladas en memoria.
- Manifiesto: 630 casos, 0 duplicados y 10 casos de `CL-LANG-015` en orden canónico.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; C++ y Python añadidos con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-015` queda completo con 10/10 casos implementados y verificados, sin bloqueos de
producto. La continuación de Ola 1 debe tomar `CL-LANG-016`.
