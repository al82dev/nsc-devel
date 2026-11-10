# T08-D21 / T08-GATE-05: propuesta de agrupación de commits de la tanda 8

Fecha: 2026-11-09. Instrucción del usuario: preparar una propuesta concreta de agrupación por cambios relacionados,
conservando los respaldos. No se reescribe la rama publicada ni se hace push forzado hasta presentarla.

## Alcance medido

- **Rango:** `97911ad25` (`interop-t07-close`) .. `HEAD` de `development-cpp-clang`. Va desde `2910c54e0`
  (`interop-t08-scope`, 2026-11-05) hasta el último commit de esta tanda.
- **Contenido:** historial lineal, sin merges. Todos los commits son de `al82dev`; no hay commits de otros
  autores intercalados. Los cuatro que no llevan prefijo `interop`/`T08` también son de la tanda:
  - `runtime-evitar-colision-fltused`;
  - `tracking-t08-tls-atomicos-calificados`;
  - los dos `docs-archivar-*-nsmeta`.
- **Tamaño:** 2399 commits al preparar esta propuesta, más los commits de cierre posteriores, que se conservan
  uno a uno.
- **Publicación:** `github/development-cpp-clang` está en `1140e1bd6`. Esos pushes no los hizo esta sesión; el
  reflog los registra como pushes externos. Los commits posteriores están solo en local.

## Método: sin reaplicar cambios

Cada commit agrupado se crea con `git commit-tree`, usando el árbol exacto del último commit original de su rango
y como padre el commit agrupado anterior. Así:

- no se reaplica ningún parche, de modo que no puede haber conflictos;
- en cada frontera de grupo el contenido es idéntico al del historial original;
- el árbol final es idéntico al de `HEAD`, y el script lo comprueba;
- el autor y la fecha de cada grupo son los de su último commit original. El cuerpo del mensaje lista todos los
  commits originales que agrupa (hash corto y asunto), para conservar la trazabilidad.

Los grupos son rangos contiguos. Reordenar por tema daría grupos más puros, pero obligaría a reaplicar parches
con conflictos y produciría estados intermedios que nunca existieron ni se compilaron. No se recomienda.

Script: `A:\nsc-interop-build\claude\d21-agrupar.py`. Sin argumentos imprime el plan. Con `--crear` crea solo la
rama local `propuesta/t08-agrupado`, sin tocar ninguna otra referencia ni hacer push.

## Grupos propuestos

**Parte 1: trabajo fino del 5 al 8 de noviembre, 8 commits en lugar de 2337.**

| # | Último commit | Commits | Fechas | Contenido |
|---|---|---|---|---|
| 1 | `d7d5cdf51` | 446 | 11-05 | auditoría inicial; contratos de tipos C: alias, `volatile` y `const` en arreglos, records, enum, globales, typedef, punteros y callbacks; gate de identidad de codegen |
| 2 | `06bd98420` | 372 | 11-05 y 11-06 | identidad y catálogo de records C (USR, empaquetados, anidados, campos), metadata C de precompilados, gate de metadata C |
| 3 | `48e3fb33d` | 469 | 11-06 | validación por familias: identidad, enum, campos de bits, uniones, prototipos, globales `const`, opacos, typedef y callbacks; validación agrupada |
| 4 | `ff07ab051` | 211 | 11-06 | TLS C en DLL y estático, selección de LLD, emisión automática, bitcode y dependencias transitivas |
| 5 | `63baabce7` | 101 | 11-06 y 11-07 | TLS atómico; validación integral del 2026-11-07 |
| 6 | `2af2921f8` | 300 | 11-07 | ABI de `bool` C, globales, punteros calificados, atómicos y enum atómico; identidad en el gate |
| 7 | `b608144cd` | 342 | 11-07 y 11-08 | plantillas con dependencias C, records anónimos, miembros flexibles, ThinLTO, modo `volatile` del consumidor |
| 8 | `7735294f3` | 97 | 11-08 | DLL TLS homónimas, rutas Unicode, callbacks calificados y globales externas (detalle de T08-002) |

**Parte 2: bloques y decisiones, uno a uno.** Desde `ca78094a8` hasta el último commit (60 al preparar la
propuesta, más los de cierre):

- validaciones de los bloques T08-002 a T08-005;
- los bloques T08-006 a T08-020;
- T08-GATE;
- las decisiones T08-D01 a T08-D24, con D13b, D11/D12, D05b y D04b.

Ya son commits de bloque o de decisión y conservan su mensaje. Solo se unen `ea45997d4` y `d63632954`: el
segundo corrige un párrafo del documento del primero.

**Resultado: 68 commits en lugar de 2399**, más los commits de cierre que se agreguen. `python d21-agrupar.py`
imprime la lista completa.

## Respaldos

- Se conservan `claude\respaldo\development-cpp-clang-20261109.bundle` y `…-20261109-noche.bundle`.
- Antes de crear la rama propuesta se guarda un bundle nuevo con la rama completa hasta el último commit de cierre:
  `claude\respaldo\development-cpp-clang-20261109-cierre.bundle`. Se verifica con `git bundle verify`.

## Decisión del usuario

1. **Aprobar los grupos** o pedir otra partición; por ejemplo, partir el grupo 3 por familia de tipos.
2. **Qué hacer con la rama agrupada:**
   - **A, recomendada:** publicarla como rama nueva (`development-cpp-clang-t08`) y conservar
     `development-cpp-clang` tal como está;
   - **B:** reemplazar `development-cpp-clang` con `git push --force-with-lease`. Hay que coordinarlo con quien
     hace los pushes externos observados, porque cualquier trabajo que salga del historial actual tendría que
     rebasarse.

Mientras no se decida, no se publica nada y `development-cpp-clang` no se toca.
