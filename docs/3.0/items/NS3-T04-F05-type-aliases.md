# NS3-T04-F05 — Type aliases and opaque aliases

## Estado

`TERMINADA`. Los alias `using` continúan siendo transparentes y `opaque type` introduce identidad
nominal, layout sin wrapper y una frontera de representación limitada al módulo propietario.

## Qué se hizo

- Se añadió la sintaxis Edition 3 `opaque type Name = Representation;`, incluidos parámetros y
  constraints genéricos.
- El preescaneo de tipos reconoce alias opacos en todos los archivos antes del parseo productivo,
  por lo que las referencias adelantadas y los ciclos llegan al modelo nominal ordinario.
- Un alias opaco baja a un `StructDeclaration` de un solo campo privado. Reutiliza name lookup,
  miembros, layout, HIR, MIR, LLVM, genéricos y NSMETA existentes; no existe backend paralelo.
- El parser genera `internal static FromRepresentation` e `internal readonly ToRepresentation`.
  Las llamadas son explícitas y no se publican en metadatos.
- El layout privado sí cruza NSMETA. Un consumidor conserva tamaño, alineación e identidad, pero no
  puede fabricar ni abrir el valor.
- Las conversiones implícitas siguen las reglas nominales y se rechazan. Los ciclos usan el
  diagnóstico determinista de struct por valor recursivo.
- Un uso `export extern C` permanece posible como escape explícito y congela la representación en
  esa frontera ABI.

## Archivos modificados

- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`: gramática, desugaring nominal y
  operaciones internas de frontera.
- `src/Frontend/ProjectCompilation.cpp`: descubrimiento previo de nombres `opaque type`.
- `CMakeLists.txt`: target dirigido exclusivo de F05.
- `A:/NATIVESHARP-3.0-TRACKING.md`: estado del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04TypeAliasTests.cpp`: seis cierres sobre ejecución O0/NSLink,
  conversiones, privacidad, ciclos, genéricos, FFI y round-trip de biblioteca.
- `docs/3.0/language/type-aliases.md`: contrato bilingüe de identidad, encapsulación y ABI.
- Este documento: evidencia atómica del commit F05.

## Mejora obtenida

Una biblioteca puede dar significado nominal a una representación barata —un entero, puntero o
agregado— sin pagar asignación ni runtime adicional y sin permitir que sus consumidores dependan
accidentalmente de la conversión interna. Los alias de compatibilidad continúan siendo totalmente
transparentes.

## Validación dirigida

- `nsc_ns3_t04_type_alias_tests`: 6/6, incluida biblioteca productora y consumidor NSMETA.
- `nsc_parser_tests`: 23/23.
- `nsc_generic_alias_tests`: 15/15.
- Regresiones T04 F01–F04: 12/12, 14/14, 16/16, 10/10 y 6/6.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
