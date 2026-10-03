# NS3-T01-F05 — Generalized initializers

Estado: terminado.

## Qué se cerró

- Se auditó el recorrido completo parser → binding → HIR → MIR → LLVM.
- Se comprobó orden textual aun cuando los nombres apuntan a slots en otro orden.
- Se comprobó evaluación única del spread y exigencia de identidad exacta de tipo.
- Se cubrieron structs readonly, structs genéricos cerrados, nesting y `T[N]`.
- Se confirmó construcción directa en el slot local, sin temporal `aggregate.init`.
- Se fijaron rechazos para duplicados, miembros omitidos, destructor de value type y agregados
  move-only/owner-bearing que todavía requerirían drop flags parciales.
- Se documentó que `T[N]` es el protocolo estático productivo de colección; las colecciones de
  usuario usarán traits nominales, no nombres mágicos impuestos por el compilador.

## Archivos

- `CMakeLists.txt`: registra la prueba dirigida independiente.
- `tests/unit/NativeSharp3T01GeneralizedInitializerTests.cpp`: nueve comprobaciones O0.
- `docs/3.0/language/generalized-initializers.md`: contrato ES/EN.
- `docs/3.0/TANDA-05-CALLS-PARAMS-INITIALIZERS-T01.md`: alinea el expediente histórico con el
  protocolo estático realmente productivo.
- `docs/3.0/items/NS3-T01-F05-generalized-initializers.md`: expediente de este ítem.

No se creó un nuevo archivo de implementación porque el vertical productivo ya estaba conectado
en `ProjectMethodBodyModel`, `ProjectOopHirModel`, `ProjectOopMirModel` y
`ProjectOopLlvmLowerer`. Duplicar ese lowering para “cerrar” el ítem habría creado dos autoridades.
El cambio material es que el contrato completo queda medido de forma independiente y ya no depende
de una fixture compartida o de ejecutar el baseline.

## Evidencia dirigida

- Target: `nsc_ns3_t01_generalized_initializer_tests`.
- Resultado: `PASS: 9 checks`.
- Compilación NativeSharp: exclusivamente `-O0`.
- Linker de programas `.ns`: NSLink local de `build-test-nslink`.
- Baseline, CTest general, O1, O2, O3 y LLD para programas NativeSharp: no ejecutados.
