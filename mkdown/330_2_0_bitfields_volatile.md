# 330 — Bitfields `volatile` / Volatile bit fields

## Resultado / Outcome

Los bitfields `volatile` ya son ejecutables. Cada lectura observa la unidad MSVC completa y cada
escritura hace read-modify-write con load y store `volatile`, conservando los campos vecinos. La
semántica aplica igual en O0/O2, structs, clases, inicializadores agregados y `LibraryReference`.

Volatile bit fields are now executable. Every read observes the complete MSVC allocation unit and
every write performs a volatile-load/volatile-store read-modify-write that preserves neighboring
fields. O0/O2, structs, classes, aggregate initialization and `LibraryReference` share one path.

## Contrato / Contract

- la unidad física, no una porción artificial, es el objeto de cada acceso LLVM `volatile`;
- `=`, `op=`, prefijo y postfijo conservan truncamiento, retorno y evaluación única del punto 329;
- los inicializadores agregados propagan la marca `volatile` al store de la unidad;
- `volatile` no significa atomic, no agrega memory order y no inserta fences;
- NSMETA 1.18 ya transportaba tanto la marca como las coordenadas y no cambia de versión;
- MIR permanece en 1.7; Object ABI 2 y Runtime ABI 1.0 tampoco cambian.

The physical unit is the volatile object; extraction and insertion happen around that observable
access. Existing assignment-result and single-evaluation rules remain intact. Volatile is not
atomic and introduces no fence or memory order. No metadata, MIR or ABI version changes are needed.

## Evidencia / Evidence

`Item330VolatileBitFieldTests.cpp` inspecciona IR O0/O2, exige load/store `volatile` de la unidad,
descarta atomics y fences, ejecuta lectura, inicialización y actualizaciones preservando vecinos, y
reconstruye un field `volatile` desde `LibraryReference`. `Item308VolatileMmioTests.cpp` protege el
contrato general de MMIO.

## Mensaje de commit / Commit message

`Implementa bitfields volatile / Implement volatile bit fields`
