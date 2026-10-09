# Tanda 01.83 - Host.Language: contratos de punteros inteligentes

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-019` |
| Casos implementados | 11/12: `TL-LANG-249`, `TL-LANG-251..260` |
| Casos bloqueados | `TL-LANG-250` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | PARCIAL POR BLOQUEO DE PRODUCTO |

La tanda comprueba las cinco familias de punteros inteligentes, la matriz completa
de operaciones observables, los límites de conteo, las restricciones entre hilos y
el contrato persistente con sus bytes, fingerprint y diagnósticos de archivo.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TL-LANG-249` | Cinco familias con nombre, threading, nullability, copy, clone y weak exactos. |
| `TL-LANG-251..252` | Move universal e implicit Copy rechazado con diagnóstico por familia. |
| `TL-LANG-253` | Clone, Downgrade y Upgrade por familia, incluidos overflows de strong/weak count. |
| `TL-LANG-254` | Upgrade no resucita strong count cero ni permite wraparound desde `UINT64_MAX`. |
| `TL-LANG-255` | Release y Reset pertenecen exclusivamente a Unique. |
| `TL-LANG-256` | Rc/WeakRc locales; las demás familias exigen payload Send/Sync y producer pinned. |
| `TL-LANG-257..258` | Manifest de 1,157 bytes y fingerprint FNV `b4cdf8ae6d2c4a10` exactos y repetibles. |
| `TL-LANG-259` | Writer con parent existente/ausente y destinos file/directory/empty. |
| `TL-LANG-260` | Verifier acepta LF/CRLF exactos y rechaza cualquier byte adicional. |

## Bloqueo confirmado

`TL-LANG-250` no puede ejecutarse honestamente con el producto vigente.
`smartPointerFamilyContract` convierte el enum externo a índice y accede directamente
a `kSmartPointerFamilies` sin validar el rango. La función además está declarada
`noexcept`. Invocar ese caso produciría comportamiento indefinido, no un contrato
observable reproducible; ejecutarlo en el host compartido también pondría en riesgo
los demás resultados. La Fase 2 no está autorizada a modificar producto, por lo que
el caso queda bloqueado hasta que exista una frontera de rechazo o un resultado
seguro definido por el producto.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido final: Debug 11/11 en 8.26 s; Release 11/11 en 0.93 s.
- Suite nueva completa final: Debug 79/79 en 479.09 s; Release 79/79 en 46.96 s.
- Controlador Python: 51/51 en 0.730 s.
- Fuentes Python de la suite: 100/100 parseadas en memoria.
- Manifiesto: 696 casos, 0 duplicados y 11 casos implementados de `CL-LANG-019`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en los dos módulos nuevos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-019` queda parcialmente cerrado con 11 casos implementados y verificados.
Su único pendiente, `TL-LANG-250`, está identificado como bloqueo de producto y no
como deuda oculta de la suite. La Fase 2 puede continuar con `CL-LANG-020` conservando
este bloqueo explícito en el cursor.
