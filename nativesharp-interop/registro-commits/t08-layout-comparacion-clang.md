# T08-010/011/012/020: comparación del layout C importado con Clang

## Propósito

Comprobar con evidencia si tamaño, alineación y desplazamiento de campos de `struct` y `union` C, tal como los usa
NativeSharp, coinciden con lo que calcula y emite Clang, en campos naturales, empaquetados, anidados, en unions y en
campos de bits, por el camino directo (encabezado importado) y por el camino precompilado (metadata).

## Alcance y SCOPE LOCK

- SCOPE LOCK completo: [t08-claude-layout-scope-lock.md](t08-claude-layout-scope-lock.md).
- Revisión de partida: `ceac3f83b4db627920c937fd1dd2ea3d4dd95908`, rama `interop-t08-claude-layout`.
- Naturaleza: prueba nueva en archivos nuevos. No se modificó código de producción, NSLink, LLVM/Clang, NSMETA,
  pruebas ni fixtures existentes.
- No cierra T08-010, T08-011, T08-012 ni T08-020 y no cierra la tanda 8.

## Cómo obtiene hoy NSC el layout (HECHO COMPROBADO en código)

- `src/Interop/Cpp/ClangSession.cpp`, `collectRecords`: tamaño y alineación del record de
  `ASTContext::getASTRecordLayout` (`getSize`, `getAlignment`); offset de cada campo de `getFieldOffset`; ancho de
  bitfield de `getBitWidthValue`. Tamaño y alineación del tipo del campo de `getTypeSize`/`getTypeAlign` del tipo
  canónico (`describeType`).
- `src/Interop/Cpp/CppSemanticRecord.cpp`, `projectCppSemanticInstanceField`: la alineación efectiva de un campo no
  bitfield NO viene de Clang; se calcula como `min(alineación natural del tipo canónico, gcd(alineación del record,
  offset en bytes))`. Para bitfields: `min(bytes de la ventana, alineación del tipo, gcd(alineación del record,
  offset))`.

## Prueba nueva: `nativesharp-interop/pruebas-tanda-08-layout-clang`

Archivos: `Api.h`, `Api.c`, `Access.ns`, `Main.ns`, `Consumer.ns`, `Producer.ns`, `Project.nsproj`,
`Producer.nsproj`, `Shared.nsproj`, `Metadata.nsproj`, `validar.cmd`, `verificar_alineacion_clang.py`.

Formas cubiertas (17 tipos, 31 accesos, 55 comparaciones de layout):

1. struct natural con padding interno y de cola (`LNatural`);
2. `pack(4)`, `pack(2)` y `pack(1)` del mismo struct (`LPack4`, `LPack2`, `LPack1`);
3. struct natural dentro de struct `pack(1)` dentro de struct natural, acceso a dos niveles (`LOuter.m.inner.a/b`);
4. struct `pack(1)` dentro de struct natural (`LHost.pi.v`, `LHost.z`);
5. arreglo de records dentro de `pack(2)` (`LPack2Array.items[1].a/b`);
6. `_Alignas(16)` en un campo, typedef con `aligned(8)` y typedef con `aligned(1)`;
7. union natural, union `pack(1)`, union dentro de struct `pack(1)`, struct dentro de union;
8. bitfields con ancho cero y sin nombre, bitfields en struct `pack(1)` con tipos distintos y bitfields en union.

Oráculos:

1. Layout: NativeSharp compara `sizeof`, `alignof` y `offsetof` de cada tipo y campo directo con los valores que
   calcula C (`l_layout`). En el consumidor sólo-metadata se usa `sizeof(*p)`/`alignof(*p)` (ver limitación 2).
2. Bytes escritos: para cada acceso, NativeSharp escribe el campo en el objeto y C escribe el mismo valor en una
   copia sombra; ambas memorias parten del mismo patrón y deben quedar idénticas byte por byte (`l_same`). Detecta
   offset, ancho, ventana de bitfield o escritura de padding incorrectos.
3. Lectura cruzada: C escribe otro valor con su función compilada por Clang y NativeSharp debe leerlo.
4. IR: `verificar_alineacion_clang.py` compara el tipo y la alineación del último `load` del getter y del último
   `store` del setter de cada acceso en el IR de NSC contra las funciones equivalentes compiladas por Clang `-O1`.
   Veredictos: `IGUAL` si NSC = `min(alineación de Clang, alineación natural del tipo IR)`; `SUBALINEADO` si es
   menor (seguro); `DEFECTO` si es mayor o si el tipo IR difiere. Los bitfields son informativos porque la ventana
   de almacenamiento puede diferir legítimamente; sus bytes los valida el oráculo 2.

Configuración ejecutada: `A:\al82dev-clang` (Clang 23.1.0) para el C de referencia y para NSC, target
`x86_64-pc-windows-msvc`, `-std=c11`; NSC construido desde `ceac3f83b` en `A:\nsc-claude-t08-layout-build\nsc`
(build hash `7b8852b99ca692c0`); `nslink.exe` existente de `A:\nsc-devel\build-clang\bin` sólo ejecutado.
`validar.cmd` acepta `NSC_T08_BUILD`, `NSC_T08_OUT` y `NSC_T08_NSLINK`; sin ellas usa las rutas habituales de
`A:\nsc-interop-build`.

## Resultados (HECHO COMPROBADO, log `A:\nsc-claude-t08-layout-build\logs\fixture-clang-7.log`)

| Camino | O0 | O1 | O2 | O3 | IR contra Clang |
|---|---|---|---|---|---|
| Directo (`Project.nsproj`) | pasó | pasó | pasó | pasó | 48 IGUAL, 14 informativos, 0 DEFECTO |
| Precompilado con encabezado (`Shared.nsproj`) | pasó | pasó | pasó | pasó | idéntico al directo |
| Precompilado sólo metadata (`Metadata.nsproj`) | pasó | pasó | pasó | pasó | idéntico al directo |

"Pasó" significa: las 55 comparaciones de layout, las 31 comparaciones byte a byte y las 31 lecturas cruzadas
devolvieron cero. Los tres reportes de alineación son idénticos (sha256 del directo:
`110d0b6105ab79cbee9ca0a9977998871054b0e0fddf53c84d278a5857c8cd41`).

Línea base adicional: la copia con rutas aisladas de `pruebas-tanda-08-layout\validar.cmd` pasó O0–O3 con el build
propio, incluidos todos sus casos negativos (log `layout-base-1.log`). Esto incluye los arreglos y la matriz de
records empaquetados (`40a0062b8`, `84549e59f`, `35e9cf099`, `764d93ba0`, `aa48d2539`): la nota de T08-010 que los
describe como "no construidos ni ejecutados" quedó superada por la actualización registrada en T08-016 y por esta
ejecución.

## Lectura de los resultados para T08-012

- Tamaño del record, alineación del record y offset de cada campo vienen directamente de Clang y coincidieron en
  los 55 casos, también al reconstruirse desde metadata.
- La alineación de acceso por campo es derivada. En los 24 campos ordinarios coincidió con
  `min(alineación de Clang, alineación natural)` en lectura y escritura, en los tres caminos.
- Clang emite alineaciones mayores que la natural cuando conoce más sobre la dirección: `LNatural.s` (8 frente a
  2), `LUnion.i` (8 frente a 4), `_Alignas(16)` (16 frente a 4) y typedef `aligned(8)` (8 frente a 4). NSC usa la
  natural: nunca excede a Clang, así que es seguro; sólo deja sin aprovechar esa información.
- No se demostró ningún acceso incorrecto. Por eso no se modificó NSMETA ni la captura: registrar en metadata la
  alineación declarada por Clang (`getDeclAlign`) no corregiría un defecto demostrado y sólo serviría para emitir
  alineaciones mayores. Queda como decisión del usuario; T08-012 sigue abierto porque la alineación por campo no se
  obtiene literalmente de Clang.

## Limitaciones de esta prueba

1. Sólo `x86_64-pc-windows-msvc`, C11 y layout MS; no se probaron otros estándares C ni otras opciones de Clang.
2. En un consumidor sólo-metadata `sizeof(LNatural)` y `alignof(LNatural)` se rechazan como identificador
   desconocido, aunque `offsetof(LNatural, d)` y `LNatural*` sí resuelven. Se analiza en el registro siguiente,
   `t08-layout-formas-pendientes.md`; aquí se mide con `sizeof(*p)`.
3. Bitfields: la ventana de NSC difiere de la de Clang (p. ej. `LBits.a`: NSC `i8 align 1`, Clang `i32 align 4`);
   los bytes coinciden, pero la equivalencia de ventana no se afirma.
4. La batería completa no se ejecutó (la integración la ejecuta Codex).
5. No hay ruta de escritura en `C:`; todo resultado quedó en `A:\nsc-claude-t08-layout-build`.

## Recursión, complejidad y austeridad

Sin código de producción. El verificador recorre una vez cada IR (lineal en su tamaño) y no usa recursión.
