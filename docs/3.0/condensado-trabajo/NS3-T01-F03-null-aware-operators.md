# NS3-T01-F03 — Operadores nulos

## Resultado

La auditoría confirmó que `??`, `??=` y el protocolo público de `Optional<T> ??` ya conservaban
short-circuit hasta LLVM, pero encontró una implementación parcial de `?.`: sólo aceptaba campos
de referencia. Getters y métodos estaban rechazados aunque figuraban en el roadmap.

El cierre añade llamadas null-conditional a la ruta común direct/virtual/interface. El receptor
se baja una vez antes de la rama; getter, argumentos, defaults y despacho se bajan únicamente en
el bloque no nulo. El resultado se une con null mediante PHI. No existe una segunda ABI de llamada.

## Cambios

- aceptación semántica de getters y grupos de métodos con `?.` cuando el resultado es anulable;
- diagnóstico causal para métodos `void` o de resultado valor, sin boxing implícito;
- análisis de argumentos bajo un join de flujo condicional;
- bit explícito `nullConditionalCall` preservado de HIR a MIR y visible en ambos dumps;
- lowering compartido para llamadas directas, virtuales y de interfaz con receptor único;
- argumentos y getters realmente diferidos en LLVM O0;
- cadenas de campos, propiedades y métodos sobre referencias anulables;
- matriz bilingüe que separa null físico de estado vacío `Optional<T>`;
- negativa de structs lookalike: los operadores no se pueden sobrecargar accidentalmente.

## Archivos

### Productivos modificados

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopHirModelDumper.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`

### Nuevos

- `tests/unit/NativeSharp3T01NullAwareOperatorTests.cpp`: vertical O0 exclusivo de F03.
- `docs/3.0/language/null-aware-operators.md`: contrato ES/EN y truth table.
- `docs/3.0/items/NS3-T01-F03-null-aware-operators.md`: expediente del ítem.

### Infraestructura modificada

- `CMakeLists.txt`: target dirigido de F03; no crea ni ejecuta baseline.

## Mejora

`?.` deja de ser azúcar limitado a layout de campos y pasa a respetar abstracción y polimorfismo.
Una propiedad con efectos y un método con argumentos costosos tienen ahora la misma garantía que
`??`: receptor único, rama diferida y cero boxing. Los dumps hacen observable ese contrato para
que una regresión no pueda convertirlo silenciosamente en evaluación eager.

## Pruebas dirigidas

- construcción local de `nsc`: correcta; contrato de runtime estático correcto.
- `nsc_ns3_t01_null_aware_operator_tests`: `PASS: 6 checks`.
- regresión `nsc_ns3_t01_expression_tests`: `PASS: 27 checks`.
- regresión `nsc_ns3_t01_named_argument_tests`: `PASS: 6 checks`.
- regresión `nsc_ns3_t01_typed_parameter_pack_tests`: `PASS: 6 checks`.
- optimización NativeSharp: sólo `O0`.
- linker de programas `.ns`: `A:\nsc-devel\build-test-nslink\bin\nslink.exe`.
- baseline, CTest completo, O1, O2 y O3: no ejecutados.
