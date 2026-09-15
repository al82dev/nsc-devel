# 329 — Lectura y escritura de bitfields / Bit-field access

## Resultado / Outcome

Los bitfields nombrados ya son valores ejecutables en structs, unions y clases. Una lectura extrae
su porción de la unidad MSVC y normaliza el tipo; una escritura actualiza sólo esa porción y conserva
los fields vecinos. La misma ruta cubre declaraciones locales, miembros promovidos e identidades
reconstruidas desde NSMETA 1.18 por `LibraryReference`.

Named bit fields are now executable values in structs, unions and classes. Reads extract and
normalize their MSVC allocation-unit slice; writes update only that slice and preserve neighboring
fields. Local, promoted and NSMETA 1.18 `LibraryReference` declarations use the same path.

## Semántica / Semantics

- unsigned usa extensión con ceros y signed extiende desde el bit de signo del field;
- `bool` se normaliza con `!= 0` y enums usan el signo de su tipo subyacente;
- `=` trunca al ancho y devuelve el valor lógico efectivamente almacenado;
- `op=`, prefijo y postfijo evalúan el receptor una vez y reutilizan la aritmética tipada existente;
- un inicializador agregado omite campos sin nombre y mezcla en orden los campos que comparten unidad;
- `pack(1)`, unions, clases y promoción anónima conservan las coordenadas del layout 328;
- un bitfield es asignable pero no direccionable: `&`, `ref`, retorno `ref`, `asm` de memoria y
  `offsetof` se rechazan;
- el acceso `volatile` se rechaza de forma explícita hasta el punto 330.

Unsigned fields zero-extend, signed fields sign-extend from their own high bit, bool fields compare
against zero, and enums follow their underlying signedness. Stores truncate to the declared width;
compound and increment/decrement forms preserve single receiver evaluation and return their proper
old or stored value. Bit fields remain non-addressable, and volatile behavior remains item 330.

## Frontera MIR/LLVM / MIR/LLVM boundary

MIR avanza a 1.7 con `BitFieldLoad` y `BitFieldAddress`. Ambos publican offset de byte, alineación y
tamaño de unidad, offset de bit y ancho. `BitFieldAddress` es un descriptor lógico consumido por
`Store`/`CompoundStore`, nunca una dirección independiente. LLVM carga la unidad entera, aplica
shift/mask y, al escribir, limpia y mezcla sólo la máscara del field antes del store completo.

MIR 1.7 carries byte/unit coordinates explicitly. LLVM therefore never guesses layout or lowers a
bit field as a narrow independent object. NSMETA, Object ABI, Runtime ABI and ownership do not
change in this item.

## Evidencia / Evidence

`Item329BitFieldAccessTests.cpp` ejecuta O0 y O2 y cubre truncamiento, preservación de vecinos,
signed wrap, `bool`, enum, asignación simple y compuesta, prefijo/postfijo, receptor con efectos,
inicializadores agregados, campos sin nombre, `pack(1)`, union, clase, promoción anónima,
`LibraryReference`, no-direccionabilidad y rechazo `volatile`. Las pruebas del contrato de toolchain
y del baseline de migración fijan MIR 1.7.

## Mensaje de commit / Commit message

`Implementa acceso a bitfields / Implement bit-field access`
