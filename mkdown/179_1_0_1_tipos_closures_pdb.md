# PDB-P4: tipos NativeSharp y closures / NativeSharp types and closures

## Resultado / Outcome

El perfil `-g` dejó de representar referencias NativeSharp como punteros a tipos
desconocidos. El productor usa ahora los modelos físicos autoritativos que ya
alimentan el ABI y publica registros CodeView consumibles sin semántica privada.

The `-g` profile no longer represents NativeSharp references as pointers to
unknown types. The producer now uses the authoritative physical models already
driving the ABI and publishes ordinary CodeView records.

## Superficies cerradas / Closed surfaces

- enums: `LF_ENUM`, underlying type y `LF_ENUMERATE`;
- structs y unions: `LF_STRUCTURE`/`LF_UNION`, tamaño, alineación y offsets;
- clases: `LF_CLASS`, campos y `LF_BCLASS` a offset cero;
- arrays y strings: puntero a cabecera `{ Length; Data[0]; }`;
- delegates: celda ABI-1 de 32 bytes con `Code`, `Receiver`,
  `ContextDestroy` y `CellFree`;
- closures: la variable capturada apunta al valor dentro de la capture cell, con
  rango real y sin ubicación fabricada si el valor desaparece;
- genéricos cerrados: nombre visible estable como `DebugFixture.Box<int>`, sin
  exponer el hash interno de materialización;
- spans, vectores, matrices, handles nominales y enumeradores construidos como
  structs/clases usan automáticamente las mismas reglas de layout.

Las interfaces permanecen como referencias opacas: NativeSharp no define un
subobjeto físico de interfaz independiente y publicar uno inventaría offsets.

Interfaces remain opaque references: NativeSharp defines no independent physical
interface subobject, so publishing one would invent offsets.

## Evidencia / Evidence

El fixture `Types.ns` cubre enum estrecho, clase base/derivada, clase genérica,
union explícita, array, string, delegate y una captura. El gate valida tipos,
miembros, offsets, rangos, checksums y líneas en O0/O2; ambos ejecutables terminan
con código 42.

Pruebas dirigidas: lowering OOP, ABI de structs, arrays/strings/Core,
lambdas/closures, ABI agregado ejecutable y gate CodeView/PDB.

## Commit

`Amplía los tipos NativeSharp en PDB / Expand NativeSharp types in PDB`
