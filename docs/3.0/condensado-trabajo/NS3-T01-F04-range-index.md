# NS3-T01-F04 — Operadores de rango e índice

## Resultado

La auditoría encontró parser y semántica parcial para `..` y `^n`, pero el rango no llegaba a una
operación ejecutable: arrays y strings lo rechazaban correctamente y cualquier tipo usuario caía
al indexador `get_Item(internal:range)`, un carrier que el lenguaje no podía declarar. También se
encontró que un índice rectangular mezclado aplicaba el `^` de la primera dimensión a todas las
dimensiones y el backend rectangular no resolvía ninguna desde el final.

El cierre conecta `receiver[start..end]` con un protocolo estructural estático `Length/Slice`,
mantiene arrays/string sin allocation implícito, y corrige `^n` por dimensión. Un binding compuesto
lleva las dos llamadas como una sola decisión semántica hasta LLVM para que el receptor se evalúe
una vez.

## Cambios

- protocolo accesible `int Length()` + `Slice(int start, int length)` para structs, generic
  structs, borrowed views y clases de despacho directo;
- rangos half-open, abiertos y extremos `^n`;
- conversión implícita de extremos a `int` y rechazo temprano de constantes negativas;
- validación runtime de `0 <= start <= end <= Length()` antes de `Slice`;
- prueba estática de `0..0`, `0..Length` y `Length..Length`, que elimina el guard desde O0;
- receptor único reutilizado para `Length` y `Slice`, incluidos temporales con efectos;
- nodo explícito `range-slice` en HIR/MIR, con targets físicos `Length` y `Slice`;
- `^0` válido como endpoint one-past y atrapado como índice de elemento;
- máscara por dimensión para `^n` en arreglos rectangulares;
- rechazo causal de protocolo incompleto, dinámico o con retornos por referencia;
- rechazo conservado de slice directo sobre heap array/string para no ocultar copias.

## Archivos

### Productivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`

### Nuevos

- `tests/unit/NativeSharp3T01RangeIndexTests.cpp`: vertical O0 exclusivo de F04.
- `docs/3.0/language/range-and-index-operators.md`: contrato ES/EN, protocolo y costos.
- `docs/3.0/items/NS3-T01-F04-range-index.md`: expediente del ítem.

### Infraestructura modificada

- `CMakeLists.txt`: target dirigido de F04; no crea ni ejecuta baseline.

## Mejora

Los rangos dejan de ser sólo sintaxis aceptada y pasan a ser una operación cero-carrier y
cero-allocation para vistas. El contrato es observable en dumps, los tipos de usuario participan
sin herencia y la evaluación única evita que `MakeView()[a..b]` consulte dos objetos distintos.
El arreglo rectangular ya no pierde la intención `^n` de cada dimensión.

## Pruebas dirigidas

- construcción local de `nsc`: correcta; contrato de runtime estático correcto.
- `nsc_ns3_t01_range_index_tests`: `PASS: 10 checks`.
- cubre borrowed generic view tipo Span, rangos abiertos/half-open, usuario, array, rectangular,
  negativos, `^0`, evaluación única, protocolo inválido y eliminación de guard demostrable.
- optimización NativeSharp: sólo `O0`.
- linker de programas `.ns`: `A:\nsc-devel\build-test-nslink\bin\nslink.exe`.
- baseline, CTest completo, O1, O2 y O3: no ejecutados.
