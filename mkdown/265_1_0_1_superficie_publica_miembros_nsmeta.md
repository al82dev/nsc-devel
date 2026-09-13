# Corrección 265 — Superficie pública de miembros NSMETA / Public NSMETA member surface

## Origen / Origin

La regeneración O0 de bindings con el compilador `0f8c8dbbbf4d5916` llegó a OpenAL y rechazó su
metadata. `AlEfx` declara campos estáticos privados de tipos `extern C delegate` internos, por
ejemplo `p_alAuxiliaryEffectSlotf`. El productor serializaba el campo privado en `metadata.members`,
pero la clausura de tipos —correctamente— no publicaba un delegate usado sólo por implementación.
El verificador detectaba entonces un contrato imposible:

```text
metadata member 'AlEfx.p_alAuxiliaryEffectSlotf'
refers to local type 'nsal_fn_alAuxiliaryEffectSlotf'
but declares neither the type nor its closed specialization
```

## Implementación / Implementation

`LibraryMetadataCodec::fromCompilation` now admits a member record only when both conditions hold:

1. the declaring type is publicly reachable through every containing type; and
2. the member itself is `public` or `protected`.

Private and internal members are no longer serialized merely because their container is public.
Consequently, a private static callback slot cannot force its internal delegate type into the public
contract. Public and protected callables, fields, properties, indexers and events keep their existing
metadata and physical identities.

## Layout y ABI / Layout and ABI

El filtro afecta `LibraryMetadataMember`, no `LibraryMetadataType.fields`. Un campo privado de
instancia que determine tamaño u offsets sigue formando parte del layout físico publicado, y la
clausura conserva cualquier tipo necesario para interpretar ese layout. Los campos estáticos no
forman parte del layout de instancia y permanecen como estado exclusivo del productor.

## Archivos / Files

- `src/Interop/LibraryMetadata.cpp`: filters member ordinals by member accessibility before records
  and type references are serialized.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: produces a DLL whose public class owns a private
  static field typed as an internal `extern C delegate`; proves the field and delegate are absent,
  the public method remains present, and DLL + LIB + NSMETA verify together.
- Compiler contract, conformance map and changelog: publish and trace NSC-META-002.

## Evidencia dirigida / Focused evidence

- `nsc_stage48_2_library_metadata_tests`: 233 checks pass.
- The focused producer emits and verifies static import library, runtime DLL and NSMETA in O0.
- No binding source was modified and no baseline was run.

## Commit de referencia / Reference commit

**Título / Title**

`Excluye implementación privada de NSMETA / Exclude private implementation from NSMETA`

**Descripción / Description**

`Serialize member records only for public and protected members of publicly reachable types, keep
private static callback state out of the public type closure, preserve physical instance layout, and
cover internal extern-C delegate fields with DLL artifact verification.`
