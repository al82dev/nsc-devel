# Tanda 01.85 - Host.Language: traits y operadores

Fecha: 2026-10-09

## Alcance implementado

| Campo | Resultado |
|---|---|
| Componente | `Host.Language` |
| Contrato | `CL-LANG-021` |
| Casos implementados | 16/16: `TL-LANG-271..286` |
| Casos bloqueados | Ninguno |
| Producto modificado | No |
| Suite histórica modificada | No |
| Estado del contrato | CERRADO |

La tanda comprueba validación y coherencia de traits, proyecciones de tipos
asociados, tipos de retorno opacos, seguridad de objetos, layout y metadata de
despacho, resolución de extensiones y el registro contractual de operadores.

## Contratos comprobados

| Casos | Comportamiento verificado |
|---|---|
| `TL-LANG-271..272` | Traits válidos y rechazo determinista del primer defecto de identidad, almacenamiento o firma. |
| `TL-LANG-273..274` | Coherencia válida, regla de orfandad y duplicados entre módulos con diagnóstico exacto. |
| `TL-LANG-275..276` | Resolución acíclica de tipos asociados y rechazo de claves duplicadas, ciclos y autociclos sin alterar el resultado previo. |
| `TL-LANG-277` | Aceptación o diagnóstico exacto para cero, uno y múltiples tipos de retorno opacos. |
| `TL-LANG-278` | Seguridad de objetos para métodos normales, default, genéricos y receptores `Self` por valor. |
| `TL-LANG-279..280` | Layout Win64 y spelling exacto de metadata para despacho estático y dinámico. |
| `TL-LANG-281` | Resolución de métodos de extensión con métodos de instancia, cero/N extensiones y frontera `SIZE_MAX`. |
| `TL-LANG-282` | Las 20 entradas del registro de operadores conservan orden y campos exactos. |
| `TL-LANG-283..284` | Lookups directo e inverso recorren todas las entradas y conservan identidad de puntero; desconocidos no resuelven. |
| `TL-LANG-285` | Proyecciones a trait/método y preferencia binaria legacy exactas. |
| `TL-LANG-286` | Sólo los spellings contractuales son sobrecargables. |

## Evidencia reproducible

- Builds reales de configuración única: Release y Debug correctos, con warnings como errores.
- Gate dirigido Debug: 16/16 en 11.64 s.
- Suite nueva completa final: Debug 81/81 en 540.63 s; Release 81/81 en 49.06 s.
- Controlador Python: 51/51 en 0.767 s.
- Fuentes Python de la suite: 102/102 parseadas en memoria.
- Manifiesto: 721 casos y 16 casos implementados de `CL-LANG-021`.
- `clang-tidy` (`clang-analyzer-*`, `bugprone-*`): 0 hallazgos en los tres módulos nuevos.
- Dependencias PE Debug y Release: únicamente `KERNEL32.dll`; CRT dinámico: 0.
- `git diff --check`: correcto; código y configuración modificados con máximo de 119 columnas.
- Diferencial bajo `include/` y `src/`: 0 archivos.
- Los seis archivos preexistentes protegidos conservaron exactamente sus SHA-256.
- No se ejecutaron NSC, NSLink, batería histórica, baseline, cobertura, instalación ni despliegue.

## Estado

`CL-LANG-021` queda cerrado con sus 16 casos implementados y verificados. No
existen bloqueos de producto ni deuda oculta de la suite para este contrato.
