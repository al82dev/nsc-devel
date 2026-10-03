# NS3-T03-F06 — C-compatible unions

## Estado

`TERMINADA`. NativeSharp 3 distingue el `union` crudo, compatible con C, del `variant`
etiquetado y exige evidencia de flujo antes de permitir una lectura segura.

## Qué se hizo

- Las declaraciones `union` de edición 3 conservan el layout superpuesto existente y reciben un
  contrato semántico de miembro activo; la edición 2 no cambia.
- `u.Campo = valor` demuestra ese campo como activo sólo después de validar el lado derecho.
- Un inicializador agregado activa el primer campo y una copia directa transporta la evidencia del
  origen al destino.
- Los joins conservan la evidencia únicamente cuando todos los caminos alcanzables coinciden en el
  mismo campo; una asignación completa sin evidencia conocida la invalida.
- Una lectura de otro campo se rechaza en código seguro con un diagnóstico que indica ambas
  soluciones válidas: escribir primero ese campo o usar `unsafe` explícitamente.
- `unsafe { ... }` se admite como bloque contextual exclusivo de edición 3 y permite la
  reinterpretación deliberada de almacenamiento superpuesto.
- La regla funciona con locales, parámetros y tipos rehidratados desde NSMETA. Receptores cuya
  identidad de almacenamiento no puede demostrarse se mantienen conservadores y requieren
  `unsafe`.

## Archivos modificados

- `src/Parser/Parser.cpp`: marca semántica para `union` de edición 3 y parse de bloques `unsafe`.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: estado de flujo del miembro activo.
- `src/Frontend/ProjectMethodBodyModel.cpp`: transferencias, invalidación, joins y diagnóstico.
- `CMakeLists.txt`: registro de la prueba dirigida del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03CUnionTests.cpp`: valida O0/NSLink, ramas, inicialización, copia,
  endian explícito, cabecera C, metadata importada y round-trip DLL.
- `docs/3.0/language/c-compatible-unions.md`: contrato bilingüe de uso seguro y FFI.
- Este documento: evidencia del commit atómico.

## Mejora obtenida

El layout C sigue siendo de costo cero y ABI-compatible, pero una reinterpretación accidental ya
no parece una lectura ordinaria segura. El compilador acepta el caso común en que una escritura
previa demuestra el campo activo y hace visible `unsafe` exactamente donde el programa decide
interpretar los mismos bytes con otro tipo.

## Validación dirigida

- `nsc_ns3_t03_c_union_tests`: 8/8 con O0 y NSLink local.
- `nsc_ns3_t03_tagged_variant_tests`: 20/20.
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6.
- `nsc_ns3_t03_exhaustive_match_tests`: 9/9.
- `nsc_ns3_t03_conditional_pattern_tests`: 6/6.
- `nsc_ns3_t03_pattern_guard_tests`: 6/6.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
