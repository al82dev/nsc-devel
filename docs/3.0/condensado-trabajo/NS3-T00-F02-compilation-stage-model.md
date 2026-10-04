# NS3-T00-F02 — Modelo de etapas de compilación

## Resultado

NSC define y aplica el orden `parse -> binding -> type checking -> comptime -> MIR ->
monomorphization -> codegen`. Cada etapa declara la información que consume y produce. El pipeline
productivo de `ProjectCompilation` registra el avance y rechaza saltos, repeticiones o productores
ausentes con `NS3010`, antes de delegar el diagnóstico semántico al backend.

## Implementación comprobada

- `CompilationStageContract` publica dependencias, productos y frontera de diagnósticos.
- `CompilationStageTracker` valida el orden y acumula información disponible.
- `ProjectCompilation` entra en las siete etapas desde las operaciones productivas reales.
- `completeCodeGeneration` cierra explícitamente la última frontera.

## Prueba dirigida

- objetivo: `nsc_ns3_t00_constitution_tests`
- resultado: `PASS: 31 checks`
- cobertura específica: salto directo a binding rechazado, secuencia completa aceptada, información
  de machine code publicada, diagnósticos de tipos independientes del backend y recorrido real hasta
  MIR/monomorfización/codegen.
- baseline: no ejecutado.

## Archivos relevantes

- `include/nativesharp/Frontend/CompilationStage.h`
- `src/Frontend/CompilationStage.cpp`
- `include/nativesharp/Frontend/ProjectCompilation.h`
- `src/Frontend/ProjectCompilation.cpp`
- `tests/unit/NativeSharp3T00ConstitutionTests.cpp`

## Mejora

Las fases dejan de ser una convención implícita: las dependencias del frontend son comprobables y
los diagnósticos no pueden aparecer accidentalmente sólo después de codegen.
