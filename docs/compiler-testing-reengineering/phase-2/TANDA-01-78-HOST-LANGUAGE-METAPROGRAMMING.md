# Tanda 01.78 - Host.Language: metaprogramación

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-014` |
| Casos implementados | 10: `TL-LANG-172..181` |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | COMPLETO |

La tanda caracteriza los contratos observables de introspección, atributos, claves derivadas,
higiene, capacidades herméticas, identidad de entradas comptime, diagnósticos de paquete y
presupuestos deterministas. Las pruebas consumen el producto existente sin cambiarlo.

## Contratos comprobados

| Caso | Comportamiento verificado |
|---|---|
| `TL-LANG-172` | Filtrado público externo, visibilidad interna y orden exacto por `sourceOrdinal`. |
| `TL-LANG-173` | La introspección no requiere metadata runtime y la respuesta es estable. |
| `TL-LANG-174` | Atributos válidos y repeatable se aceptan sin mutar valores ni orden. |
| `TL-LANG-175` | Identity/target vacíos y duplicado no-repeatable producen el primer error exacto. |
| `TL-LANG-176` | Campos desordenados, duplicados y vacíos producen bytes canónicos length-prefixed. |
| `TL-LANG-177` | Spelling y scope iguales o distintos determinan la identidad higiénica exacta. |
| `TL-LANG-178` | Matriz completa de cinco capacidades en el perfil hermético. |
| `TL-LANG-179` | Entradas se ordenan por identity; payloads y duplicados permanecen en la identidad. |
| `TL-LANG-180` | Matriz de package/code válida, vacía, ajena y reservada `NS3`. |
| `TL-LANG-181` | Límites de steps, bytes y depth, contadores, error y atomicidad ante fallo. |

## Propiedades adicionales verificadas

- La introspección no muta la secuencia entregada por el llamador.
- La validación exitosa limpia errores anteriores y conserva ambas retenciones.
- Las claves derivadas conservan duplicados y representan identidades vacías explícitamente.
- La identidad comptime mantiene payloads con separadores sin reinterpretarlos.
- Un consumo rechazado no descuenta ningún contador del presupuesto.
- `depth` es un límite por invocación y no un contador acumulativo.
- No se introdujo recursión.

## Evidencia reproducible

- Build dirigido `nsc_component_tests`: Debug y Release correctos, con warnings como errores.
- Gate dirigido: Debug 10/10 en 5.66 s; Release 10/10 en 0.63 s.
- Suite nueva completa: Debug 74/74 en 350.82 s; Release 74/74 en 37.94 s.
- Controlador Python: 51/51 en 0.762 s.
- Fuentes Python de la suite: 95/95 compiladas en memoria.
- Manifiesto: 620 casos, 0 duplicados y 10 casos de `CL-LANG-014` en orden canónico.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en código de usuario.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; C++ y Python añadidos con máximo de 120 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-014` queda completo con 10/10 casos implementados y verificados, sin bloqueos de
producto. La continuación de Ola 1 debe tomar `CL-LANG-015`.
