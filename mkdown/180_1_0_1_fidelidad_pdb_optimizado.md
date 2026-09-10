# PDB-P5: fidelidad bajo optimización / Optimized-code fidelity

## Resultado / Outcome

El perfil completo conserva en O2 solamente las ubicaciones que LLVM puede
demostrar válidas. El PDB distingue valores constantes, valores en registro o
stack, agregados fragmentados, rangos con huecos y valores eliminados. Los inline
sites conservan sus variables anidadas y sus sitios de llamada.

The full profile retains at O2 only locations LLVM can prove valid. The PDB
distinguishes constants, register and stack values, fragmented aggregates,
ranges with gaps, and optimized-away values. Inline sites retain their nested
variables and call sites.

## Corrección de capturas / Capture fix

Una captura usada por una lambda tenía una declaración sobre su dirección real,
pero el optimizador no podía transportar esa dirección indirecta hasta CodeView
y la marcaba como eliminada. Al cargarla, el productor emite ahora también el
valor SSA real. Esto genera un rango de registro tanto en la lambda independiente
como cuando se inlinea, sin prolongar el lifetime más allá de la carga.

A used capture had a declaration on its real address, but the optimizer could
not carry that indirect address into CodeView and marked it optimized away. On
load, the producer now also emits the real SSA value. This creates a register
range in both the standalone and inlined lambda without extending the lifetime
beyond the load.

## Evidencia / Evidence

El gate O2 exige:

- constantes `answer` y `mode` con valor 42;
- `label` identificado explícitamente como optimizado fuera;
- `capturedTypeValue` con `S_DEFRANGE_REGISTER` y sin bandera de eliminación;
- ubicaciones de registro y stack, fragmentos de agregados y rangos con huecos;
- inline sites para una llamada ordinaria y para una lambda.

El mismo ejecutable conserva el resultado observable 42. Las columnas nacen de
las posiciones fuente de cada `DILocation`; CodeView puede omitirlas cuando no
aportan una distinción adicional. No se crean discriminadores, tail frames ni
funciones artificiales sólo para simular equivalencia con O0.

## Commit

`Endurece el PDB para código optimizado / Harden PDB for optimized code`
