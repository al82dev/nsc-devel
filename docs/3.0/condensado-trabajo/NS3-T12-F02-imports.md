# NS3-T12-F02 — Imports explícitos y aliases / Explicit imports and aliases

## Español

NativeSharp 3 acepta directivas explícitas de módulo, símbolo, trait, extensiones y glob
restringido. Las formas productivas son:

```nativesharp
import module Demo.Core;
import module Demo.Core as Core;
import symbol Demo.Core.Value;
import symbol Demo.Core.Value as V;
import trait Demo.Contracts.Readable;
import extensions Demo.Extensions;
import Demo.Public.*;
```

El parser no crea una segunda ruta de resolución. Cada import se baja a las directivas de
namespace o alias que consume `ProjectNameLookup`; por eso tipos genéricos, traits y extensiones
usan la misma escala de nombres existente. Todas las directivas del scope se recolectan antes de
resolver declaraciones, de modo que el resultado no depende del orden textual.

Los aliases de símbolo y trait son locales al scope. Si se omite `as`, el último segmento es el
nombre local. Dos imports que reclaman el mismo alias se rechazan. Los imports de extensiones y
glob no aceptan alias porque incorporan un conjunto de candidatos, no una declaración local.

Validación puntual: `nsc_ns3_t12_import_tests`, 5/5 checks. La prueba compila y enlaza un proyecto
real con `-O0` y NSLink, ejecuta el resultado esperado 42, verifica normalización determinista y
rechaza una colisión real.

## English

NativeSharp 3 accepts explicit module, symbol, trait, extension, and restricted-glob imports.
They use the productive forms shown above.

The parser does not introduce a second name-resolution path. Each import lowers to the namespace
or alias directives already consumed by `ProjectNameLookup`, so generic types, traits, and
extensions share the existing lookup ladder. A scope collects every directive before binding its
declarations, making lookup independent from textual order.

Symbol and trait aliases are local to their scope. When `as` is omitted, the final identity
segment becomes the local name. Two imports claiming the same alias are rejected. Extension and
glob imports cannot have aliases because they contribute a candidate set rather than one local
declaration.

Focused validation: `nsc_ns3_t12_import_tests`, 5/5 checks. It compiles and links a real `-O0`
project with NSLink, executes the expected result 42, verifies deterministic normalization, and
rejects a real collision.
