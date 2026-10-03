# NS3-T05-F02 — Implementations and coherence

## Estado

`TERMINADA`. `impl Trait for Type` está conectado al parser, lookup, miembros, herencia,
interfaces, genéricos, monomorfización, NSMETA y backend productivos.

## Qué se hizo

- Se añadió `impl Trait for Type { ... }` como sintaxis contextual exclusiva de edition 3.
- Un bloque se integra como parte del tipo concreto: sus witnesses entran al modelo de miembros y
  su trait a las tablas existentes. No existe un runtime, ABI ni layout paralelo.
- Los witnesses sin accesibilidad escrita se vuelven públicos; accesibilidad no pública y miembros
  con storage/lifetime propio se rechazan.
- Se eligió coherencia sellada: sólo el módulo propietario de una clase local no estática puede
  escribir su impl. Un consumidor puede implementar un trait importado para su tipo local, pero
  no una clase extranjera.
- `ProjectInterfaceModel` verifica que el contrato sea realmente un trait y diagnostica el mismo
  par declarado dos veces o simultáneamente en lista base y bloque `impl`, con nota al sitio previo.
- Los impls genéricos exigen parámetros, argumentos objetivo y constraints idénticos a la clase.
  Se fusionan antes de recolectar plantillas y sus members/type syntax se clonan normalmente.
- El registro reutilizable conserva los bloques fuente anexos. NSMETA concatena y canonicaliza
  referencias externas dentro de cada segmento para que el consumidor vuelva a aplicar la misma
  fusión antes de monomorfizar.
- `TypeSyntax` conserva la procedencia `explicit trait implementation` al clonar, permitiendo que
  coherence opere después de resolver identidades reales y no por comparación textual tardía.

## Archivos modificados

- `include/nativesharp/AST/TypeSyntax.h`, `src/AST/TypeSyntax.cpp`: marca de procedencia de impl.
- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`: gramática, miembros y restricciones.
- `src/Frontend/ProjectNameLookup.cpp`: unión con la clase propietaria y frontera orphan sellada.
- `include/nativesharp/Frontend/ProjectGenericModel.h`,
  `src/Frontend/ProjectGenericModel.cpp`: impl condicional, fusión y clonación.
- `src/Frontend/ProjectInterfaceModel.cpp`: validación trait/duplicados y diagnósticos causales.
- `src/Interop/LibraryMetadata.cpp`: fuente reutilizable multisegmento para impl genérico.
- `CMakeLists.txt`: target dirigido de F02.
- Documentos maestros T05 y tracking: estado real del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T05ImplCoherenceTests.cpp`: prueba productiva O0/NSLink con positivos,
  negativos, módulos separados, privacidad, condicionales y round-trip NSMETA.
- `docs/3.0/language/trait-implementations.md`: contrato de usuario ES/EN.
- Este documento: alcance, archivos, mejora y evidencia del commit atómico.

## Mejora obtenida

La conformidad ya no necesita vivir dentro de la declaración de clase, pero continúa siendo global,
determinista y verificable sin un registry runtime. Un build incremental o limpio y un consumidor
precompilado observan un único dueño y una única implementación. La separación facilita adaptar
tipos propios a traits de bibliotecas sin permitir extensiones huérfanas incompatibles.

## Validación dirigida

- Construcción incremental de `nsc`: correcta; runtime host estático verificado.
- `nsc_ns3_t05_impl_coherence_tests`: 11/11.
- Parser: 23/23.
- Declaraciones trait F01: 14/14.
- Genéricos T04: 12/12.
- Metadata Stage 48.2: 336/336.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
