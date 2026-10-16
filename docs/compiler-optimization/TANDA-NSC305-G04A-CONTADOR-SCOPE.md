# Tanda NSC305-G04a — Contador de copias de scope (instrumentación para G04)

> Campaña NativeSharp Compiler 3.0.5. Único producto: `nsc`.
> Instrumentación previa a la refactorización de scopes persistentes.

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo | Añadir contadores `scopeCopies` / `scopeCopiedEntries` a `GenericTelemetry` y registrarlos en la copia de `GenericLookupScope` de `processNode`, para cuantificar el costo que motiva G04. |
| Naturaleza | Instrumentación aditiva (no-op si la telemetría está apagada). |
| Comportamiento idéntico | Todo; sólo añade contadores cuando `--time` activa la telemetría. |
| Revisión de referencia | `development` en `b491563f`. |
| Archivos | `include/nativesharp/Frontend/ProjectGenericModel.h`, `src/Frontend/ProjectGenericModel.cpp`. |
| Símbolos | `GenericTelemetry` (+`scopeCopies`, `scopeCopiedEntries`, `noteScopeCopy`), `processNode`, `renderSummary`. |
| Pruebas | `Host.Frontend` completo (544 casos). |

## 2. Cambio

- `GenericTelemetry`: contadores `scopeCopies` y `scopeCopiedEntries` + `noteScopeCopy(entries)`.
- `processNode`: registra el número de entradas de contenedor copiadas antes de cada `GenericLookupScope currentScope = scope;`.
- `renderSummary`: expone `scope-copies` y `scope-copied-entries`.

## 3. Evidencia (escalado, fixture N structs genéricos)

| N | root-demand-discovery (us) | factor |
|---|---:|---:|
| 100 | 31.356 | — |
| 200 | 94.921 | 3,03× |
| 400 | 351.177 | 3,70× |
| 500 | 691.134 | 1,97× |

`root-demand-discovery` escala ≈ N^1,8 (super-lineal), consistente con la copia O(N²) de `valueTypes`
que acumula los locals de la unidad. Esto **justifica** la refactorización de scopes persistentes (G04).

- **HECHO COMPROBADO**: `Host.Frontend` **544 PASS / 0 FAIL** tras la instrumentación.

## 4. Siguiente acción

NSC305-G04b: convertir `GenericLookupScope` a frames delta parent-linked (empezando por `valueTypes`,
el contenedor que escala con los locals), manteniendo el sombreado y con prueba focal de escalado.
