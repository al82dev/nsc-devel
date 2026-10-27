# Congelamiento 3.0, elemento 140 — NSMETA acepta un getter de propiedad por referencia

Fecha: 2026-10-27. Revisión de partida: `31d4721e4`.

## Nota de trabajo

```text
HEAD: 31d4721e4
Elemento: 140 del plan (defecto que ocultaba el 139 en NativeSharp.Core.Collections.Generic)
Contrato vigente: P3 (doc 28, `ref T Current` de los enumeradores); NSMETA 1.15 (retorno por referencia)
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  src/Interop/LibraryMetadataVerification.cpp
  tests/unit/Freeze30ContractTests.cpp
  docs/3.0/NSC-ESTADO-CONGELAMIENTO-3.0.md
  docs/3.0/decisiones-congelamiento/60-NSMETA-GETTER-DE-PROPIEDAD-POR-REFERENCIA.md
```

## Problema

P3 hizo que el `Current` de los enumeradores devolviera por referencia (`public ref T Current { get; }`). Una
biblioteca que publica una especialización cerrada de uno de ellos no se puede emitir.
`verifyReferenceReturnMemberMetadata` sólo acepta el retorno por referencia en un método ordinario o en un
getter de indexador. Con el nsc de `31d4721e4`, `NativeSharp.Core.Collections.Generic` (sonda
`A:\temp\nsc-dec\d45\cg.cmd`) falla así:

```text
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification
failed: metadata member 'type:NativeSharpCoreCollectionsGeneric::NativeSharp.Core.Collections.Generic.
LinkedListEnumerator__g1__string__h5ed9428842eb9426.get_Current()' carries ref-return outside an ordinary method
or indexer getter
```

Antes del 139 no se veía: la verificación de identidad física fallaba primero.

## Contrato

- La verificación acepta el retorno por referencia en un getter de propiedad, igual que en uno de indexador.
- Sigue rechazándolo en:
  - un setter;
  - un accesor que no es getter;
  - un miembro que no es método.
- La superficie pública ya declaraba la propiedad como `ref T Nombre { get; }` (`emitAccessorContract`), y
  `validateAccessorContract` ya rechaza un setter junto a un getter por referencia.
- El formato no cambia.

## SCOPE LOCK

- **Cambia.** Sólo la condición de `verifyReferenceReturnMemberMetadata` y el texto de su diagnóstico («…outside
  an ordinary method or a property or indexer getter»).
- **Idéntico.**
  - La escritura y la lectura de NSMETA.
  - La superficie pública.
  - El consumidor.
  - El resto de la verificación.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol.

Sondas `A:\temp\nsc-dec\d45`, en `-O0`:

- **`cg.cmd`.**
  - Construye `NativeSharp.Core` como biblioteca.
  - Construye las fuentes de `NativeSharp.Core.Collections.Generic` contra ella. `cgsrc\CG.nsproj` usa una
    `LibraryReference` en vez del `ProjectReference` del compilador, que mete `Core` en la misma biblioteca.
  - La biblioteca emite.
  - `--dump-library-abi` da nombre físico a los seis constructores públicos de las especializaciones publicadas:
    `List<string>()`, `List<string>(int)`, `LinkedList<string>()`, `LinkedListNode<string>()`,
    `ListEnumerator<string>(List<string>)` y `LinkedListEnumerator<string>(LinkedList<string>)`.
- **C2** sale con 121. Corre `new List<string>()`, `new List<string>(5)`, `new LinkedList<string>()` y
  `OwnedStringCollections.AddCopy`/`AddLastCopy`.
- **C3** sale con 51. Recorre con `foreach (ref string ...)` el `List<string>` y el `LinkedList<string>`
  publicados, a través de su `ref T Current`.
- **`ref.cmd`.** LRef publica `Cell<int>` con `ref T Value { get; }`. El consumidor C4 lee y escribe por él y
  sale con 499.

Con el nsc de `31d4721e4` (antes de este elemento), L3 falla igual que `Collections.Generic`, en
`Cell__g1__int__h64f7f9e69c042f4d.get_Value()`. Se midió con la prueba Freeze30 de abajo.

Prueba nueva: `Freeze30ContractTests::testReferenceReturnPropertyLibrary` (L3 y C4). El binario completo en
`A:\temp\nsc-dec\bt`:

- Este árbol: pasan 1082 de 1088 comprobaciones, F140Library entre ellas.
- Con la verificación de `31d4721e4`: falla además F140Library («carries ref-return outside an ordinary method
  or indexer getter»).

Las seis que fallan en los dos árboles son las del entorno de `bt` (doc 59).

Resto: `seq.cmd f140` contra `f139` es idéntico en todo.

## No verificado

- Batería completa y unitarias: gate final.
- Un `ref readonly` en un getter de propiedad publicado (la condición es la misma; no tiene sonda propia).
