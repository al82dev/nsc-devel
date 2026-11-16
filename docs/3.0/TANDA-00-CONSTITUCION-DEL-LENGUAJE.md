# NativeSharp — Tanda 00: Constitución del lenguaje

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

> **Reclasificación 2026-09-29:** `TARGETED-GREEN`, pendiente del baseline integral de Alberto. La conexión
> productiva de etapas, categorías, efectos, NSMeta, cache, LSP y conformidad se documentó en
> [TANDA-04-CONSTITUCION-EJECUTABLE-T00.md](TANDA-04-CONSTITUCION-EJECUTABLE-T00.md).

Estado técnico: implementación histórica de T00. Plataforma normativa: Windows x64. La evidencia de esta tanda corresponde a la configuración que se indica abajo; la validación vigente debe cubrir O0, O1, O2 y O3 cuando corresponda.

La identidad del producto es `1.0.0`. El lenguaje es único y acumulativo.

## Español

### Lenguaje único

El frontend aplica las mismas reglas a todos los proyectos. La propiedad histórica `<Language>`, si aparece, no selecciona semántica ni debe provocar rechazos. Los grafos de referencias se comprueban por sus contratos y capacidades reales; no por una etiqueta de edición. Native ABI y Runtime ABI conservan sus contratos propios.

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

`CompilationStageTracker` rechaza saltos, repeticiones y ciclos con `NS3010`. El `comptime` semántico pertenece a esta etapa; el preprocesamiento heredado que normaliza fuentes antes del parser no se considera el `comptime` normativo de T11.

### Categorías de valores

| Categoría | Significado |
|---|---|
| `place` | almacenamiento identificable y potencialmente modificable |
| `value` | valor puro sin identidad de almacenamiento exigible |
| `temporary` | valor materializado con lifetime delimitado |
| `borrowed-value` | place observado mediante préstamo |
| `moved-from` | origen consumido que no puede reutilizarse |

Los nombres históricos `LValue` y `RValue` permanecen como aliases de `Place` y `Value`. Esto permite migrar el frontend por capas sin mantener dos sistemas semánticos.

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

| Código | Causa | Ejemplo mínimo | Corrección |
|---|---|---|---|
| `NS3010` | transición inválida entre etapas | entrar a binding sin parse | ejecutar el productor anterior y no saltar etapas |
| `NS3020` | operación inválida para la categoría de valor | leer un owner después de `move(owner)` | reasignar el storage o dejar de usar el valor movido |
| `NS3030` | contexto de efecto requerido ausente | asignar heap dentro de `noheap` | quitar la operación o el contrato, o usar storage permitido |
| `NS3049` | superficie reconocida pero todavía diferida | una construcción reservada llega a un lowering no habilitado | usar una forma admitida o habilitar su implementación completa |
| `NS3099` | manifest de conformidad inválido | target distinto de `windows-x64` | corregir el esquema, target y resultados esperados |

`error`, `warning`, `lint` y `note` son severidades diferentes. Un diagnóstico puede llevar un span primario, spans secundarios con etiqueta y notas estructuradas. La presentación conserva la identidad de cada código; `NS3xxx` es una familia de códigos diagnósticos, no una versión del lenguaje. Los antiguos diagnósticos de selección de edición no deben utilizarse para rechazar código.

### Harness de conformidad

`tools/ns_conformance.py` consume manifests JSON con:

- esquema e identidad del caso;
- clase `normative` o `regression`;
- target obligatorio `windows-x64`;
- comando y timeout;
- exit code, stdout, stderr y diagnósticos esperados;
- checks opcionales de MIR y ABI;
- número de repeticiones para determinismo.

Cada repetición usa un directorio temporal nuevo. El harness distingue `passed`, `failed`, `invalid`, `timeout` y `nondeterministic`. Su autocheck ejecuta casos positivos, manifest inválido, timeout, salida no determinista y aislamiento.

## English

### Single language

The frontend applies one set of language rules to all projects. A historical `<Language>` property, if present, does not select semantics and must not cause rejection. Reference graphs are checked against actual contracts and capabilities; ABI and library contracts remain separate.

### Compilation stages

The normative order is parse, binding, type checking, semantic comptime, MIR, monomorphization, and codegen. Each stage declares its required and produced information. `CompilationStageTracker` rejects missing producers, skipped stages, repetitions, and cycles with `NS3010`. Frontend diagnostics remain independent from the backend.

### Value categories

The official categories are place, value, temporary, borrowed value, and moved-from. The historical `LValue` and `RValue` spellings remain compatibility aliases for place and value. Helper contracts define place behavior, temporary materialization, and moved-from rejection.

### Effect contexts

The shared effect representation covers unsafe, throws, async, comptime, and noheap. Nested contexts inherit explicit capabilities. Throws, async, and noheap participate in function types; unsafe and comptime are contextual. Later batches add syntax and full analysis on top of this representation.

### Stable diagnostics and conformance

The catalog includes `NS3010` for a missing producer stage, `NS3020` for an invalid value operation, `NS3030` for an operation forbidden by an effect, `NS3049` for recognized but deferred surface, and `NS3099` for an invalid conformance manifest. Historical edition-selection diagnostics must not reject code. Error, warning, lint, and note are distinct. Structured diagnostics support primary and secondary spans plus notes. The conformance harness runs normative and regression manifests in isolated directories, enforces timeouts and deterministic repetition, and verifies stdout, diagnostics, MIR, and ABI evidence.

## Archivos creados y modificados

- Nuevos contratos públicos en `include/nativesharp/Language`, `Frontend`, `Sema` y `Diagnostics`.
- Integración histórica en `Project`, `ProjectLoader`, `ProjectGraphLoader`, dumps y unidades frontend; la selección por edición fue retirada.
- Harness nuevo `tools/ns_conformance.py`.
- Prueba dirigida `tests/unit/NativeSharpT00ConstitutionTests.cpp`.
- Registro de ambos checks específicos en `CMakeLists.txt`.

## Mejora obtenida

Las características comparten orden de etapas, semántica de valores, propagación de efectos, formato de errores y evidencia. T00 documenta la base de estos contratos.

## Validación de cierre

- `nsc_ns3_t00_constitution_tests`: compilado y aprobado, 31/31 checks.
- `tools/ns_conformance.py --self-test`: aprobado, 5/5 checks.
- dos manifests normativos ejecutados contra el `nsc.exe` O0 construido: 2/2.
- target `nsc`: compilado correctamente.
- Se comprobó la identidad pública utilizada entonces; no es evidencia de la identidad actual.
- `NSC_BUILD_TESTS` se restauró a `OFF` después de las pruebas dirigidas.
- No se ejecutó baseline general ni CTest completo.
