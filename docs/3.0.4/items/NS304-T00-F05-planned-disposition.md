> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# NS304-T00-F05 — Disposición `Planned`

## Estado

`CERRADO` el 2026-10-12. La disposición distingue una capacidad comprometida
por el plan 3.0.4 de una capacidad productiva. No habilita sintaxis ni reduce el
criterio de implementación definido por el plan.

## SCOPE LOCK

- Objetivo: impedir que “incluida en el plan” se interprete como
  “implementada”.
- Productor: `LanguageSurface`.
- Consumidores: manifest del Driver, diagnóstico del Parser y pruebas de
  contrato de `Host.Language`.
- Protegido: implementación de las nueve features, runtime, CMake, toolchain,
  Builder y configuración.
- Revisión base: `67ee19257` en `development`.

## Contrato

`LanguageDisposition` conserva cuatro estados distintos:

| Disposición | Label de manifest | Significado productivo |
|---|---|---|
| `Implemented` | `implemented` | disponible; exige vertical completa |
| `Planned` | `planned-3.0.4` | comprometida, pero todavía rechazada |
| `Deferred` | `deferred-4.0` | fuera del alcance 3.0.4 |
| `Rejected` | `rejected-3.0` | no soportada por decisión contractual |

La transición a `Implemented` no ocurre en T00. Cada feature sólo puede
promoverse después de completar su tanda causal y la definición obligatoria de
“implementado” del plan 3.0.4.

## Reconciliación del catálogo

| Estado | Features | Total |
|---|---|---:|
| `Implemented` | record structs, primary constructors, raw strings, extension methods, trait operators, move-only defaults | 6 |
| `Planned` | async/await, async streams, yield generators, default interface implementations, list patterns, static constructors, decimal, condition declarations, static locals | 9 |
| `Deferred` | ninguna entrada actual | 0 |
| `Rejected` | long double Win64 | 1 |

El Parser conserva el rechazo mediante `NS3049` para las nueve entradas
planificadas. El texto ahora informa que la capacidad está planeada para 3.0.4
pero no implementada; la presencia en el manifest nunca equivale a aceptación.

## Evidencia puntual

- Reconstrucción enfocada del target existente `nsc_component_tests` bajo
  LLVM23: 3 objetos afectados y un enlace; sin regenerar CMake.
- `TL-LANG-155..161`: **7/7 PASS**.
- `TL-LANG-155`: tabla exacta de 16 entradas y disposiciones.
- `TL-LANG-158`: bytes y labels exactos del manifest.
- `TL-LANG-159`: sólo seis entradas productivas.
- `TL-LANG-160`: nueve planes no productivos, cero diferidas y un rechazo.
- `TL-LANG-161`: manifest y diagnósticos deterministas.
- T49 fue actualizado como consumidor, pero no ejecutado: sus árboles
  existentes pedían regenerar CMake, operación excluida por el SCOPE LOCK.

## Gate F05

```text
catalog_entries = 16
implemented = 6
planned_3_0_4 = 9
deferred_4_0 = 0
rejected_3_0 = 1
planned_reported_as_implemented = 0
focused_cases = 7/7 PASS
```

Siguiente acción determinista: `NS304-T00-F06`, congelar versiones previstas
de AST, HIR, MIR, NSMETA, ABI runtime y debug metadata.
