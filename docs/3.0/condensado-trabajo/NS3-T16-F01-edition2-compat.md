# NS3-T16-F01 — Compatibilidad por contenido / Content-based compatibility

## Español

El frontend utiliza un único lenguaje acumulativo y no selecciona semántica por edición. Los
módulos deben tener identidades de fuente y módulo válidas y sus imports deben resolverse. Para
contratos binarios, una estructura inválida o un target no admitido se rechazan; una huella ABI
distinta requiere un adaptador explícito. Un cambio de huella de fuente invalida el módulo y sus
consumidores incrementales. NSMeta no se acepta ni se rechaza por versión.

Las comprobaciones existentes se encuentran en `CompatibilitySemantics.h`,
`CompatibilitySemantics.cpp` y `NativeSharp3T16Edition2CompatTests.cpp`. El nombre de esta última
es un identificador histórico del archivo, no una selección de lenguaje. La validación del estado
actual corresponde al usuario.

## English

The frontend uses one cumulative language and does not select semantics by edition. Modules need
valid source and module identities and resolved imports. Structurally invalid binary contracts or
unsupported targets are rejected; a different ABI fingerprint requires an explicit adapter. A
source fingerprint change invalidates the module and its incremental consumers. NSMeta is not
accepted or rejected by version.

Existing checks are in `CompatibilitySemantics.h`, `CompatibilitySemantics.cpp`, and
`NativeSharp3T16Edition2CompatTests.cpp`. The latter filename is a historical identifier, not a
language selection. The user will validate the current state.
