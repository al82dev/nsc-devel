# Tanda NSC305-B00/B01 — Instrumentación y coordinator de method bodies

> Campaña NSC 3.0.5. Rama `development`. Base: `dcf38eb9b`.

## SCOPE LOCK

- B00 conserva la instrumentación crash-surviving bajo una etiqueta estable, pero la deshabilita
  para producción después del cierre del defecto de heap/stack.
- B01 mueve físicamente `ProjectMethodBodyModel::build` a
  `Frontend/Bodies/MethodBodyBuildCoordinator.cpp` sin cambiar su cuerpo.
- Los únicos helpers compartidos son la traza deshabilitada y la construcción pura de
  `PointerContract`.
- Se preservan orden de diagnósticos, worklists, waves, merge, ordinals, dumps, ABI y NSMETA.
- No se autorizan cambios de sintaxis, semántica, fixtures, manifests, baseline, deploy o instalación.

## Gates

1. `nsc_project_method_body_model_tests` debe conservar sus 76 comprobaciones, incluidas equivalencia
   exacta j1/j2/j12 y dump estable.
2. `nsc` Release debe compilar.
3. El perfil focal c23 O0/g0 j16 debe mantener el resultado semántico y reducir el costo artificial
   de `method-bodies`.
4. `git diff --check` debe pasar; cada cierre produce commit local aislado.

## Implementación

- La traza conserva la etiqueta `[NSC METHOD BODIES INSTRUMENTATION CODE]`, la ruta de evidencia y
  su protocolo de flush, pero `kTraceEnabled=false` permite que el compilador elimine todo el I/O.
- `MethodBodyInternals.h` contiene sólo esa instrumentación y la construcción pura de
  `PointerContract`; no contiene estado semántico.
- `ProjectMethodBodyModel::build` se movió literalmente al nuevo translation unit coordinator.
  Sigue operando sobre la misma instancia y llama los mismos métodos privados en el mismo orden.
- CMake registra el nuevo archivo. No se agregaron virtuales, interfaces públicas, allocations ni
  conversiones de datos.
- El selector focal de L05 volvió a `int main()` para respetar el generador oficial de adapters; se
  activa con `NSC_PROJECT_OOP_LOWERING_FOCUS` y usa `_dupenv_s` bajo el CRT de Windows.

## Evidencia

Fecha: 2026-10-17.

- `nsc_project_method_body_model_tests`: PASS 76/76, exit 0. Incluye dump y equivalencia exacta
  j1/j2/j12.
- `nsc_project_oop_lowering_tests` con foco de invalidación: PASS 13/13, exit 0.
- La reconfiguración CMake y la generación de adapters pasan con `int main()` restaurado.
- Perfil c23 warm j16, cinco muestras, O0/g0:

| Métrica | Antes B00 | Después B00 | Cambio |
|---|---:|---:|---:|
| method-bodies mediana | 246.281 us | 85.323 us | -65,4 % |
| total mediana | 1.130 ms | 964 ms | -14,7 % |
| LLVM mediana | 452.361 us | 430.832 us | estable/mejora 4,8 % |

Todas las muestras warm conservaron 12 hits y 0 misses. Evidencia:
`A:\temp\nsc305-b00-profile\measurements.csv` y logs subordinados.

## Resultado

B00 y B01 quedan cerrados. Se eliminó trabajo de disco ajeno a la compilación y el coordinator ya
tiene frontera física propia. B02–B08 pueden extraerse por familias sin volver a modificar la
entrada pública ni el scheduler. No se ejecutó baseline, suite completa, deploy ni instalación.
