# Congelamiento 3.0 P3 (DEC-12) — retorno `ref readonly`

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `737503602`.

## Propósito y problema

P3 pide accesos por referencia a los elementos de las colecciones, con esta restricción: la referencia es no
modificable cuando mutarla rompería invariantes. Las claves de diccionarios, los elementos de sets y sus
equivalentes no pueden exponerse como `ref` mutable.

El lenguaje solo tenía retornos `ref T`, que son asignables (`cell[0] = 30` escribe por el alias). Los únicos
alias de solo lectura eran locales: `foreach (ref T x in …)` y los patrones `ref`. No había forma de que un
método, una propiedad o un indexador devolviera una referencia que no se pudiera escribir.

Decisión de Alberto Huerta Luna (P3, 2026-10-26): accesos y enumeración por referencia, no modificables donde
mutar rompería invariantes.

## SCOPE LOCK

- **Objetivo.**
  - `ref readonly T` como retorno de un método, propiedad o indexador.
  - El resultado es un lugar direccionable que no se puede escribir. Se comporta como un parámetro `in`:
    lecturas, miembros `readonly` y `foreach (ref …)`.
- **Naturaleza.** Extensión del lenguaje necesaria para P3.
- **Debe quedar idéntico.**
  - `ref T` (mutable), sus pruebas y su ABI.
  - Toda la superficie sin `ref readonly`.
- **Modificable.**
  - AST: `ReadonlyReferenceMarker.h` (nuevo, solo funciones).
  - Parser: `Parser.h`, `MemberParser.cpp`.
  - Frontend:
    - `ReadonlyReferenceReturn.h` (nuevo);
    - `CallAnalyzer.cpp`, `IdentifierAnalyzer.cpp`, `MemberAccessAnalyzer.cpp`;
    - `IndexAndRangeExpressions.cpp`, `CollectionExpressions.cpp`, `StatementAnalyzer.cpp`;
    - `ProjectInterfaceModel.cpp`, `ProjectVirtualDispatchModel.cpp`.
  - NSMETA: `LibraryMetadata.h` (bit 31), `LibraryMetadataConstruction.cpp`, `LibraryMetadataVerification.cpp`.
  - `PublicSurfaceBuilder.cpp`.
  - `Freeze30ContractTests.cpp`.
- **Prohibido.**
  - Cambiar HIR, MIR o LLVM: el ABI es el mismo que el de `ref T`.
  - Cambiar el esquema de la caché C1 o del snapshot.
- **Terminación.**
  - Sondas R en `-O0` y `-O2`.
  - 48 casos `TL-LANG` de referencias, préstamos y vistas en Debug y Release.

## Contrato

1. **Sintaxis.** `ref readonly T` (y `ref readonly['a] T`) antes del tipo de retorno de un método, una
   propiedad o un indexador, de clase o de interfaz. En un campo, un bit-field o un operador da «'ref readonly'
   is valid only on the return of a method, property or indexer».
2. **Representación.**
   - La declaración y su accesor get llevan el marcador `readonly-reference` en el texto auxiliar.
   - `isReference()` sigue diciendo que el callable devuelve una dirección.
   - El texto auxiliar ya sobrevive al clonado genérico, a la caché C1 y a la unidad de API precompilada, así
     que ningún formato interno cambia.
   - `isReadonlyReferenceReturn(member)` lo deriva de la sintaxis.
3. **Semántica.** El resultado de llamar a uno de estos miembros es:
   - `addressable` y no `assignable`;
   - `readonlyReceiver`, igual que un parámetro `in`.

   Por eso las reglas existentes rechazan:
   - la asignación;
   - el local `ref` mutable;
   - el paso a un parámetro `ref`;
   - la llamada a un miembro mutante de struct;
   - la escritura de un campo;
   - `foreach (ref mut …)`.

   `foreach (ref …)` lo acepta.
4. **`return ref`.** En un método `ref readonly` puede devolver almacenamiento que el método solo puede leer.
   Las demás reglas de escape y de vida no cambian.
5. **Interfaces y overrides.** El modo `ref readonly` forma parte de la firma. Implementar o sobreescribir con
   el otro modo se rechaza.
6. **NSMETA.**
   - El bit 31 de las banderas de miembro (`kMetadataMemberReadonlyReferenceReturn`) acompaña al bit 24.
   - La verificación lo rechaza sin el bit 24.
   - La superficie pública lo reconstruye como `ref readonly`.
   - Es un cambio de contrato de NSMETA. El incremento de época de P8 lo cubrirá.

## Validación

nsc Release, `nsc_component_tests` Debug y Release recompilados (exit 0). Sondas en `A:\temp\nsc-dec\d14`:

| Sonda | Forma | Resultado |
|---|---|---|
| R1b | lectura por método, indexador y propiedad `ref readonly`; escritura por `ref` mutable | 7990 (`-O0` y `-O2`) |
| R11InterfaceOk | interfaz `ref readonly int Get()` implementada igual | 3 |
| R13bForeachField | `foreach (ref int x in b)` con `ref readonly int Current` | 123123 |
| R2Write | `c.Get() = 5` | «left side of assignment is not assignable» |
| R3RefLocal | `ref int x = ref c.Get()` | «reference-local initializer must be a mutable addressable storage location» |
| R5Mutate | `h.Get().Bump()` sobre un struct | «readonly receiver cannot invoke mutating struct member 'Bump'» |
| R6FieldWrite | `h.Get().A = 3` | no asignable |
| R8IndexerWrite, R9PropertyWrite | escritura por indexador y propiedad `ref readonly` | no asignable |
| R10Interface | `ref readonly` implementado con `ref` | «does not implement interface method 'Get()'» |
| R12Field | `ref readonly` en un campo | rechazado |
| R14bForeachFieldMut | `foreach (ref mut …)` sobre `ref readonly Current` | rechazado |

Casos `nsc_component_tests` (Debug y Release, todos PASS): TL-LANG-051, 072, 078–081, 147, 223–226, 302, 309, 310,
313, 316, 357–362, 364–366, 374, 391–393, 402, 408, 409, 434, 440–442, 445–451, 459–461, 475 y 509.

## Defectos encontrados (anteriores, no corregidos aquí)

1. **Receptor struct de un retorno `ref`.** `h.GetMut().A = 5` y `h.GetMut().Bump()` actúan sobre una copia
   temporal: R16a da 0 y R16c da 0. Con un local `ref` sí funciona (R16b da 1). El HIR materializa
   `temporary-address(pointer-load(instance-call reference-result=true))`.
2. **Biblioteca precompilada.** Un método `ref` público no se puede consumir: la superficie generada
   `public ref[this] int Get()` da «names undeclared lifetime 'this'» (RLib2Use). Afecta igual a `ref readonly`
   (RLibUse).
3. **Reenvío `in`.** `B(in y)` con `y` un parámetro `in` no encuentra sobrecarga (R18InForward). Un resultado
   `ref readonly` tampoco se puede pasar a un parámetro `in` (R1Method).
4. **Vida de un elemento.** `return ref data[i]` con `data` un arreglo campo, incluso `owned`, da «requires lifetime
   'this' but the returned storage has lifetime 'unknown'» (R7Indexer). Un indexador `ref` de colección no se puede
   escribir todavía.

Cada uno va en su propio commit causal antes de los accesos por referencia de la biblioteca.

## No verificado

- `Freeze30ContractTests` no está configurado en los árboles locales; `testReadonlyReferenceReturns` repite las
  sondas.
- Batería completa: queda para el cierre final.
