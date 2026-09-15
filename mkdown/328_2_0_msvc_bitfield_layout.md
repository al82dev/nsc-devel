# 328 — Layout de bitfields MSVC / MSVC bit-field layout

## Resultado / Outcome

NativeSharp asigna layout físico Windows x64 a bitfields de structs, unions y clases. La unidad
usa el tamaño del tipo base declarado, campos consecutivos del mismo tamaño comparten bits mientras
quepan y el offset avanza desde el bit menos significativo. Cambiar el tamaño, agotar la unidad o
encontrar un field ordinario abre una unidad nueva.

NativeSharp now assigns Windows x64 physical layout to struct, union and class bit fields. The
allocation unit uses the declared base type's full size; equal-sized consecutive fields share it
while they fit, with offsets increasing from the least-significant bit.

## Reglas MSVC / MSVC rules

- bases permitidas: enteros, `bool` y enums;
- ancho constante, no negativo y dentro del tipo base;
- sólo una declaración sin nombre admite ancho cero;
- una barrera cero inicial no afecta layout; después de una unidad viva la termina y alinea;
- `pack(N)` limita alineación sin reducir el tamaño de la unidad;
- cada alternativa de union comienza en byte/bit cero y aporta su unidad al extent;
- las clases reutilizan la misma autoridad después de su header o base;
- `offsetof` y el acceso ejecutable permanecen rechazados para bitfields.

Allowed bases are integer, bool and enum types. Widths are compile-time nonnegative values bounded
by the base. Zero-width barriers, packing, unions and class prefixes follow the measured MSVC x64
model. Executable field access remains the separate item 329.

## Fronteras C y NSMETA / C and NSMETA boundaries

El header C/MSVC reproduce declaraciones nombradas, sin nombre y de ancho cero, y no intenta usar
`offsetof` sobre ellas. NSMETA avanza a 1.18 y conserva flags, unidad, offset de bit y ancho. Una
escritura downlevel 1.17 se rechaza porque perdería el contrato; el lector actual conserva el golden
histórico 1.17 y una `LibraryReference` materializa nuevamente la declaración fuente.

The generated host header spells the physical declarations without illegal `offsetof` checks.
NSMETA 1.18 carries the exact coordinates and rejects a lossy pre-1.18 downgrade; precompiled
consumers reconstruct the same declaration and layout.

## Evidencia / Evidence

`Item328MsvcBitFieldLayoutTests.cpp` cubre unidades compartidas y partidas, tipos signed/unsigned
del mismo tamaño, ancho cero inicial y efectivo, `pack(1)`, unions, clases, diagnósticos, header,
round-trip NSMETA, rechazo downlevel y reconstrucción mediante `LibraryReference`. El layout bajo
nivel fue contrastado además con `clang-cl` para el ABI Microsoft x64.

## Mensaje de commit / Commit message

`Implementa layout MSVC de bitfields / Implement MSVC bit-field layout`
