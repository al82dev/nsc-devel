# Tanda 01.77 - Host.Language: modelo de memoria

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-013` |
| Casos implementados | 10: `TL-LANG-162..171` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

La tanda caracteriza las reglas observables del modelo de memoria actual: decodificación de
órdenes, validez por operación, compare-exchange, data races, atomicidad x64, `volatile` y
el manifiesto público. Las pruebas consumen el producto existente sin cambiar sus contratos.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-162` | Valores ABI 0..4 decodifican exactamente; 5 y `uint64_t` máximo retornan `nullopt`. |
| `TL-LANG-163` | `Load` acepta relaxed, acquire y seq-cst; rechaza release y acquire-release. |
| `TL-LANG-164` | `Store` acepta relaxed, release y seq-cst; rechaza acquire y acquire-release. |
| `TL-LANG-165` | `ReadModifyWrite` acepta los cinco órdenes públicos. |
| `TL-LANG-166` | `Fence` rechaza relaxed y acepta los cuatro órdenes restantes. |
| `TL-LANG-167` | Matriz 5x5 completa de órdenes success/failure de compare-exchange. |
| `TL-LANG-168` | Un data race conserva `UndefinedBehavior` en consultas repetidas. |
| `TL-LANG-169` | Sólo 8, 16, 32 y 64 bits naturalmente alineados son lock-free en Windows x64. |
| `TL-LANG-170` | `volatile` no sincroniza y puede rasgarse si está desalineado o supera 64 bits. |
| `TL-LANG-171` | Bytes completos, orden, etiquetas, newline y determinismo del manifest. |

## Propiedades adicionales verificadas

- La matriz de compare-exchange recorre las 25 combinaciones y comprueba su resultado exacto.
- Los anchos 0, 8, 16, 32, 64 y 128 se prueban tanto alineados como desalineados.
- Los casos límite externos no dependen de truncamientos ni conversiones implícitas.
- El manifest se compara completo y dos invocaciones consecutivas producen el mismo contenido.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 10/10 en 5.40 s; Release 10/10 en 0.63 s.
- Suite nueva completa: Debug 73/73 en 329.25 s; Release 73/73 en 36.17 s.
- Controlador Python: 51/51 en 0.702 s.
- Fuentes Python de la suite: 94/94 compiladas en memoria.
- Manifiesto: 610 casos, 0 duplicados y 10 casos de `CL-LANG-013` en orden canónico.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; C++ y Python añadidos con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-013` queda completo con 10/10 casos implementados y verificados, sin bloqueos de
producto. La continuación de Ola 1 debe tomar `CL-LANG-014`.
