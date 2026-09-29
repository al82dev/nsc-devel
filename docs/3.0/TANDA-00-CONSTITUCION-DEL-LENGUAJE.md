# NativeSharp 3.0 — Tanda 00: Constitución del lenguaje

> **Reclasificación 2026-09-29:** `TARGETED-GREEN`, pendiente del baseline integral de Alberto. La conexión
> productiva de edición, etapas, categorías, efectos, NSMETA, cache, LSP y conformidad se cerró en
> [TANDA-04-CONSTITUCION-EJECUTABLE-T00.md](TANDA-04-CONSTITUCION-EJECUTABLE-T00.md).

Estado técnico: implementación de T00. Plataforma normativa: Windows x64. Política de pruebas: `-O0` para productos NativeSharp. Esta tanda no introduce sintaxis 3.0; fija los contratos que las tandas siguientes deben respetar.

La identidad pública del compilador de esta rama es `3.0.0`; NativeSharp 2.0 permanece congelado en su rama y distribución independientes.

## Español

### Ediciones del lenguaje

Un proyecto selecciona explícitamente su edición en el `.nsproj`:

```xml
<PropertyGroup>
  <Language>3</Language>
</PropertyGroup>
```

Los únicos valores válidos son `2` y `3`. Si la propiedad falta, se conserva la edición 2 para mantener compatibilidad con proyectos existentes. La edición controla reglas del frontend; no cambia por sí sola Native ABI, Runtime ABI ni la versión de una biblioteca.

Cada `Project`, `ProjectSyntaxUnit` y compilación frontend conserva la edición elegida. El dump del proyecto la publica. Un grafo de `ProjectReference` no puede mezclar ediciones: el error estable es `NS3001`. La identidad textual de edición es distinta para que una herramienta incremental invalide su resultado al cambiarla.

### Modelo normativo de etapas

| Orden | Etapa | Requiere | Produce | Diagnóstico necesita backend |
|---:|---|---|---|---|
| 1 | parse | — | sintaxis | no |
| 2 | binding | sintaxis | símbolos | no |
| 3 | type-checking | sintaxis y símbolos | tipos | no |
| 4 | comptime | tipos | constantes | no |
| 5 | MIR | tipos | flujo de control | no |
| 6 | monomorphization | tipos y MIR | especializaciones | no |
| 7 | codegen | MIR y especializaciones | código de máquina | sí |

`CompilationStageTracker` rechaza saltos, repeticiones y ciclos con `NS3010`. El `comptime` semántico de 3.0 pertenece a esta etapa; el preprocesamiento heredado que hoy normaliza fuentes antes del parser no se considera todavía el `comptime` normativo de T11.

### Categorías de valores

| Categoría | Significado |
|---|---|
| `place` | almacenamiento identificable y potencialmente modificable |
| `value` | valor puro sin identidad de almacenamiento exigible |
| `temporary` | valor materializado con lifetime delimitado |
| `borrowed-value` | place observado mediante préstamo |
| `moved-from` | origen consumido que no puede reutilizarse |

Los nombres 2.0 `LValue` y `RValue` permanecen como aliases de `Place` y `Value`. Esto permite migrar el frontend por capas sin mantener dos sistemas semánticos.

### Contextos de efectos

| Efecto | Se hereda en contextos anidados | Forma parte del tipo de función |
|---|---:|---:|
| `unsafe` | sí | no |
| `throws` | sí | sí |
| `async` | sí | sí |
| `comptime` | sí | no |
| `noheap` | sí | sí |

`EffectContext` responde qué operaciones están autorizadas y devuelve la identidad exacta del efecto faltante. T00 no decide todavía la sintaxis de cada efecto ni implementa todos sus análisis; establece una sola representación para las tandas posteriores.

### Diagnósticos estables

T00 reserva el catálogo inicial:

| Código | Causa |
|---|---|
| `NS3001` | ediciones incompatibles dentro del grafo de módulos |
| `NS3002` | característica 3.0 usada bajo edición 2 |
| `NS3010` | transición inválida entre etapas |
| `NS3020` | operación inválida para la categoría de valor |
| `NS3030` | contexto de efecto requerido ausente |
| `NS3099` | manifest de conformidad inválido |

`error`, `warning`, `lint` y `note` son severidades diferentes. Un diagnóstico puede llevar un span primario, spans secundarios con etiqueta y notas estructuradas. Los diagnósticos anteriores a 3.0 conservan su representación textual histórica; sólo los códigos `NS3xxx` se muestran entre corchetes junto a la severidad.

### Harness de conformidad

`tools/ns3_conformance.py` consume manifests JSON con:

- esquema e identidad del caso;
- clase `normative` o `regression`;
- edición `2` o `3`;
- target obligatorio `windows-x64`;
- comando y timeout;
- exit code, stdout, stderr y diagnósticos esperados;
- checks opcionales de MIR y ABI;
- número de repeticiones para determinismo.

Cada repetición usa un directorio temporal nuevo. El harness distingue `passed`, `failed`, `invalid`, `timeout` y `nondeterministic`. Su autocheck ejecuta casos positivos, manifest inválido, timeout, salida no determinista y aislamiento.

## English

### Language editions

A project selects edition 2 or 3 with `<Language>` in its `.nsproj`; omission keeps edition 2 compatibility. Editions select frontend rules, not ABI or library versions. Project-reference graphs reject silent edition mixing with `NS3001`, compilation units retain the edition, project dumps expose it, and edition fingerprints differ for incremental invalidation.

### Compilation stages

The normative order is parse, binding, type checking, semantic comptime, MIR, monomorphization, and codegen. Each stage declares its required and produced information. `CompilationStageTracker` rejects missing producers, skipped stages, repetitions, and cycles with `NS3010`. Frontend diagnostics remain independent from the backend.

### Value categories

The official categories are place, value, temporary, borrowed value, and moved-from. The 2.0 `LValue` and `RValue` spellings remain compatibility aliases for place and value. Helper contracts define place behavior, temporary materialization, and moved-from rejection.

### Effect contexts

The shared effect representation covers unsafe, throws, async, comptime, and noheap. Nested contexts inherit explicit capabilities. Throws, async, and noheap participate in function types; unsafe and comptime are contextual. Later batches add syntax and full analysis on top of this representation.

### Stable diagnostics and conformance

Codes `NS3001`, `NS3002`, `NS3010`, `NS3020`, `NS3030`, and `NS3099` form the first stable 3.0 catalog. Error, warning, lint, and note are distinct. Structured diagnostics support primary and secondary spans plus notes. The conformance harness runs normative and regression manifests in isolated directories, enforces timeouts and deterministic repetition, and verifies stdout, diagnostics, MIR, and ABI evidence.

## Archivos creados y modificados

- Nuevos contratos públicos en `include/nativesharp/Language`, `Frontend`, `Sema` y `Diagnostics`.
- Integración de edición en `Project`, `ProjectLoader`, `ProjectGraphLoader`, dumps y unidades frontend.
- Harness nuevo `tools/ns3_conformance.py`.
- Prueba dirigida `tests/unit/NativeSharp3T00ConstitutionTests.cpp`.
- Registro de ambos checks específicos en `CMakeLists.txt`.

## Mejora obtenida

Las características 3.0 ya no tendrán que inventar por separado su versión de lenguaje, orden de etapas, semántica de valores, propagación de efectos, formato de errores o evidencia. T00 deja una base única, comprobable e incrementalmente identificable antes de introducir sintaxis nueva.

## Validación de cierre

- `nsc_ns3_t00_constitution_tests`: compilado y aprobado, 31/31 checks.
- `tools/ns3_conformance.py --self-test`: aprobado, 5/5 checks.
- dos manifests normativos ejecutados contra el `nsc.exe` O0 construido: 2/2.
- target `nsc`: compilado correctamente.
- identidad pública `3.0.0` comprobada.
- `NSC_BUILD_TESTS` se restauró a `OFF` después de las pruebas dirigidas.
- No se ejecutó baseline general ni CTest completo.
