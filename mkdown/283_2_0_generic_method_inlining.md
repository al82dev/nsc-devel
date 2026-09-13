# 283 — Inlining de métodos genéricos / Generic method inlining

## Resumen / Summary

Las políticas `inline`, `forceinline` y `noinline` sobreviven la monomorfización de métodos
genéricos. El resultado es idéntico para llamadas con argumentos de tipo explícitos o inferidos.

The `inline`, `forceinline`, and `noinline` policies survive generic-method monomorphization. The
result is identical for calls with explicit or inferred type arguments.

## Archivos y funciones / Files and functions

- `src/Frontend/ProjectGenericModel.cpp`, `cloneNode()` — documenta y conserva los modificadores
  semánticos en cada clon cerrado.
- `tests/unit/GenericInliningTests.cpp` — materializa tres métodos genéricos, mezcla inferencia y
  argumentos explícitos, inspecciona MIR/LLVM y comprueba el resultado O0.
- `CMakeLists.txt` — registra la prueba focalizada `nsc.language.generic-inlining`.
- `docs/1.0.1/` — actualiza especificación, contrato, mapas, libro, estado y roadmap.

## Implementación y decisiones / Implementation and decisions

La infraestructura de especialización ya clonaba todos los modificadores AST antes de construir el
modelo de miembros. Se conservó esa única ruta y se hizo explícita su obligación; duplicar la
política dentro del registro genérico crearía dos autoridades susceptibles de divergir.

The specialization infrastructure already cloned every AST modifier before member-model
construction. That single route remains authoritative and its obligation is now explicit;
duplicating the policy in the generic record would create two authorities that could diverge.

La prueba exige funciones físicas cerradas y no se conforma con ver el token en la plantilla. Cada
especialización debe presentar la política correcta en OOP MIR, emitir su atributo LLVM y respetar
su comportamiento bajo el pipeline O0. También confirma que la cache canónica no distingue una
especialización inferida de la misma especialización solicitada explícitamente.

The test requires closed physical functions rather than merely observing the template token. Each
specialization must expose the correct OOP MIR policy, emit its LLVM attribute, and honor its O0
behavior. It also confirms that the canonical cache does not distinguish an inferred specialization
from the same explicitly requested specialization.

## Evidencia dirigida / Focused evidence

`Preferred<int>`, `Required<int>` y `Forbidden<int>` se materializan y contienen respectivamente
`Preferred`, `Required` y `Forbidden`. Antes de optimizar existen las tres llamadas; en O0 sólo se
expande `Required<int>`.

`Preferred<int>`, `Required<int>`, and `Forbidden<int>` materialize with `Preferred`, `Required`, and
`Forbidden`, respectively. All three calls exist before optimization; only `Required<int>` expands
at O0.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Conserva inlining en métodos genéricos / Preserve inlining on generic methods`

**Descripción / Description**

`Certifica que la monomorfización explícita e inferida conserva las políticas inline, forceinline y noinline hasta LLVM y su comportamiento O0. / Certifies that explicit and inferred monomorphization preserves inline, forceinline, and noinline policies through LLVM and their O0 behavior.`
