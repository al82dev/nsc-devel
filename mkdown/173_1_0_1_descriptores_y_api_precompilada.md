# 1.0.1: descriptores y API de bibliotecas precompiladas / Precompiled library descriptors and API

## Resumen / Summary

**ES.** Este cambio corrige dos rechazos producidos por NSC al consumir artefactos `.lib` +
`.nsmeta` que el propio compilador había emitido y verificado:

1. una clase local no podía usar `NativeSharp.Core.Object` (u otra clase) como base precompilada,
   porque el lowering buscaba el descriptor externo entre los globals locales;
2. el importador reconstruía un `extern C` no exportado como si fuera un método NativeSharp
   publicado y después exigía un nombre mangled inexistente.

**EN.** This change fixes two NSC rejections while consuming `.lib` + `.nsmeta` artifacts that the
compiler itself had emitted and verified:

1. a local class could not use precompiled `NativeSharp.Core.Object` (or another class) as its
   base because lowering searched for the external descriptor among local globals;
2. the importer reconstructed a non-exported `extern C` import as a published NativeSharp method
   and later demanded a nonexistent mangled name.

## Diagnóstico reproducido / Reproduced diagnosis

**ES.** La reducción mínima construyó una biblioteca `NativeSharp.Core`, verificó correctamente sus
artefactos y falló al compilar un consumidor que sólo llamaba `MathUtil.Max(4, 42)`:

```text
NativeSharp library artifacts: valid
nsc: error: Macro A descriptor cannot resolve its base descriptor
```

La presencia de `NativeSharp.Core.Object` activa la raíz opcional C8. Por ello `Program`, aun sin
base explícita, hereda de un tipo precompilado. El ABI NSMETA 1.6+ publica
`__nsc_get_classdesc_*`; no promete que el símbolo de datos `__nsc_classdesc_*` sea importable desde
un DLL.

Tras corregir esa ruta, el paquete DirectX real expuso el segundo rechazo:

```text
metadata-backed NativeSharp call resolved to unpublished symbol
'__nsc_method_...DirectXSupport__nsdx_copy_from...'
```

`nsdx_copy_from` es una declaración `extern C` que pertenece a la implementación nativa del
productor. No es `export extern C`, por lo que no forma parte de la API NativeSharp precompilada.

**EN.** The minimal reduction built a `NativeSharp.Core` library, successfully verified its
artifacts, and failed while compiling a consumer that only called `MathUtil.Max(4, 42)`. The
presence of `NativeSharp.Core.Object` activates the optional C8 root, so `Program` inherits from a
precompiled type even without an explicit base. NSMETA 1.6+ publishes `__nsc_get_classdesc_*`; it
does not promise that the `__nsc_classdesc_*` data symbol can be imported from a DLL.

After that path was corrected, the real DirectX package exposed the second rejection.
`nsdx_copy_from` is an `extern C` declaration owned by the producer's native implementation. It is
not `export extern C`, so it is not part of the precompiled NativeSharp API.

## Archivos y funciones modificados / Modified files and functions

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

- `emitDispatchMetadata`:
  - identifica todos los descriptores importados;
  - propaga esa condición a descendientes locales;
  - declara todos los accessors locales antes de definirlos;
  - deja nulo inicialmente el campo base de descriptores cuya ascendencia es precompilada;
  - obtiene la base mediante el accessor publicado y la instala con `cmpxchg` secuencialmente
    consistente;
  - mantiene constantes los descriptores que sólo tienen ascendencia local.
- `lowerNew` usa el accessor cuando el tipo objetivo es importado o tiene ascendencia importada.
- `runtimeDescriptorAccessorTypes_` conserva el conjunto exacto de tipos que requieren esa ruta.

The atomic compare/exchange publishes the identical base pointer once when two threads race to
construct the first object. It avoids repeated unsynchronized writes while preserving the PE-safe
function-based descriptor ABI.

### `src/Driver/Driver.cpp`

- `canMaterializePrecompiledDeclaration` excluye `extern C` cuando no lleva `export`.
- `export extern C` sigue siendo materializable: el productor sí publica su entrada NativeSharp y
  su thunk C estable.

This prevents generated `.nsmeta.public-api.ns` source from inventing an imported NativeSharp
method for a producer-only native dependency.

### `tests/unit/Stage46NativeLibrariesTests.cpp`

- agrega un productor mínimo con `NativeSharp.Core.Object` y `MathUtil`;
- agrega un consumidor cuya clase `Program` recibe implícitamente la base precompilada y debe
  ejecutar con código 42;
- agrega un import `extern C` público no exportado al productor general y certifica que no contamina
  la superficie reconstruida del consumidor;
- conserva cobertura de bibliotecas estáticas y DLL en O0/O2.

## Evidencia / Evidence

Compilador focalizado / Focused compiler:

```text
nsc 0.9.0+build.20260909T195447Z.5f25b44c71435b60
```

Pruebas ejecutadas / Tests executed:

```text
nsc_project_oop_lowering_tests:        71/71
nsc_stage46_native_libraries_tests:   145/145
nsc_stage48_2_library_metadata_tests:  94/94
nsc.stage56.c8-type-root:                1/1
CoreConsumer real: compile=0, run=42
```

El paquete DirectX real ya supera lectura de metadata, reconstrucción de API y descriptores. Ahora
alcanza `lld-link`, donde reporta dependencias nativas no incluidas en el artefacto probado
(`D3D11CreateDevice`, `nscom_*`, `nsdx_copy_from`). Ese empaquetado es un frente separado y no se
oculta en este cambio.

The real DirectX package now passes metadata reading, API reconstruction, and descriptors. It
reaches `lld-link`, which reports native dependencies absent from the tested artifact
(`D3D11CreateDevice`, `nscom_*`, `nsdx_copy_from`). That packaging issue is separate and is not
hidden by this change.

No se ejecutó el baseline completo para este lote; se reserva para el cierre de una entrega mayor.
The full baseline was not run for this batch; it remains reserved for a larger delivery close.

## Mensaje de commit de referencia / Reference commit message

### Título / Title

```text
Corrige descriptores y API precompilada / Fix precompiled descriptors and API
```

### Descripción / Description

```text
ES: Resuelve bases de clases precompiladas mediante accessors PE-safe y publica la cadena con
cmpxchg cuando una clase local hereda de NativeSharp.Core.Object u otro tipo externo. Evita además
que imports extern C no exportados se reconstruyan como métodos NativeSharp inexistentes. Agrega
regresiones de bibliotecas estáticas, DLL, Object implícito y dependencias nativas privadas.

EN: Resolve precompiled class bases through PE-safe accessors and publish the chain with cmpxchg
when a local class inherits from NativeSharp.Core.Object or another external type. Also prevent
non-exported extern C imports from being reconstructed as nonexistent NativeSharp methods. Add
regressions for static libraries, DLLs, the implicit Object root, and private native dependencies.
```
