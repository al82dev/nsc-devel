# NS3-T12-F04 — Re-exports y fachadas / Re-exports and facades

## Español

NativeSharp 3 incorpora re-exports productivos con `export import`. La forma de símbolo/trait
puede publicar un nombre de fachada:

```nativesharp
export import symbol Core.Api.Original as Value;
export import trait Core.Contracts.Readable as Readable;
export import module Core.Public;
```

`ProjectNameLookup` construye un catálogo por módulo después de cerrar todas las declaraciones y
antes de bindear usos. La entrada pública apunta al ordinal del tipo original: no crea un tipo,
una especialización, un símbolo COFF ni una instancia nueva. Por ello mangling, documentación,
ABI y linker conservan la identidad del productor.

Un re-export sólo puede publicar una cadena completamente `public`. Un nombre que choca con una
declaración local se rechaza. Dos fachadas en diamante que apuntan al mismo ordinal se deduplican;
si el mismo nombre apunta a identidades diferentes, el uso es ambiguo de forma determinista.
`export import module` conserva las identidades públicas del namespace seleccionado.

Validación puntual: `nsc_ns3_t12_reexport_tests`, 5/5 checks. La prueba compila y enlaza a O0 con
NSLink, ejecuta la implementación original con resultado 42, acepta un diamante idéntico y
rechaza fachadas en conflicto.

## English

NativeSharp 3 provides productive re-exports through `export import`. Symbol and trait forms may
publish a facade name, while module form republishes the selected namespace's public identities.

`ProjectNameLookup` builds one module catalogue after declarations are finalized and before uses
are bound. Each public facade entry points to the original type ordinal; it creates no type,
specialization, COFF symbol, or runtime instance. Mangling, documentation identity, ABI, and
linkage therefore remain owned by the original producer.

Only an entirely public containment chain may be re-exported. Local declaration collisions are
rejected. Diamond facades targeting the same ordinal deduplicate; a shared public spelling that
targets different identities is deterministically ambiguous.

Focused validation: `nsc_ns3_t12_reexport_tests`, 5/5 checks. It compiles and links with O0 and
NSLink, executes the original implementation with result 42, accepts an identical diamond, and
rejects conflicting facades.
