# 172 — Origen declarador en especializaciones externas / Declaration origin in external specializations

## Resultado / Outcome

Un proyecto consumidor ya puede crear la primera especialización de un genérico definido en otra
biblioteca usando tipos declarados en un tercer proyecto. Casos reales como
`Dictionary<string, Texture2D>`, `Dictionary<string, Song>` y
`Dictionary<string, ShaderProgram>` dejan de buscar esos tipos dentro de Core y alcanzan OOP MIR,
LLVM, COFF y ejecución nativa.

A consumer project can now create the first specialization of a generic defined in another
library using types declared in a third project. Real shapes such as
`Dictionary<string, Texture2D>`, `Dictionary<string, Song>`, and
`Dictionary<string, ShaderProgram>` no longer look those types up inside Core and reach OOP MIR,
LLVM, COFF, and native execution.

Este cambio cierra la causa de `NSC-LIMIT-023`. No añade almacenamiento alternativo ni exige que
el productor fuerce una especialización artificial.

This change closes the cause of `NSC-LIMIT-023`. It adds no alternate storage and does not require
the producer to force an artificial specialization.

## Causa / Cause

`ProjectGenericModel` ya resolvía la identidad canónica de un argumento escrito en un proyecto
referenciado. Esa identidad sólo se usaba para construir la clave de monomorfización. Al clonar
el cuerpo de la plantilla, `preserveOrAttachInstantiationOrigin` marcaba el `TypeSyntax`
sustituido con el proyecto y namespace del sitio que escribió la especialización.

Eso basta para `List<Payload>` cuando `Payload` pertenece al propio consumidor, pero no cuando:

1. Core declara `Dictionary<TKey, TValue>`;
2. una biblioteca declara `Texture2D`;
3. un consumidor visible a ambas crea por primera vez `Dictionary<string, Texture2D>`.

El clon vive dentro de la unidad de Core. El origen incorrecto hacía que name lookup buscara
`Texture2D` en el consumidor y después en Core, aunque el pase genérico ya había identificado la
declaración pública correcta en la biblioteca de tipos.

`ProjectGenericModel` already resolved the canonical identity of an argument written from a
referencing project. That identity was used only to build the monomorphization key. When cloning
the template body, `preserveOrAttachInstantiationOrigin` tagged the substituted `TypeSyntax` with
the project and namespace of the specialization request site.

That is sufficient for `List<Payload>` when `Payload` belongs to the consumer itself, but not when
Core declares the generic, a library declares the argument type, and a third project requests the
first specialization. The clone lives in Core's syntax unit, so the wrong origin made name lookup
retry the type in the consumer and Core despite the generic pass having already selected the
correct public declaration.

## Archivos modificados / Files modified

### `include/nativesharp/Frontend/ProjectGenericModel.h`

- `DeclaredTypeIdentity` conserva ahora el namespace de la declaración además de proyecto,
  escritura fuente e identidad estable.
- `DeclaredTypeOrigin` representa el contexto mínimo que name lookup necesita para volver a
  enlazar un tipo sustituido sin interpretar su texto otra vez.
- `resolvedTypeOriginBySyntax_` acompaña a `resolvedTypeIdentityBySyntax_`: una tabla protege la
  identidad de especialización y la otra protege el sitio autoritativo de resolución.
- `resolveDeclaredTypeIdentity` devuelve la declaración seleccionada, no sólo una copia de su
  cadena de identidad.

- `DeclaredTypeIdentity` now keeps the declaration namespace in addition to project, source
  spelling, and stable identity.
- `DeclaredTypeOrigin` represents the minimum context name lookup needs to bind a substituted
  type again without reinterpreting its text.
- `resolvedTypeOriginBySyntax_` accompanies `resolvedTypeIdentityBySyntax_`: one table protects
  specialization identity and the other protects the authoritative lookup site.
- `resolveDeclaredTypeIdentity` returns the selected declaration rather than only a copy of its
  identity string.

### `src/Frontend/ProjectGenericModel.cpp`

`collectTemplates` registra el namespace declarador. `processTypeSyntax` guarda juntos la clave
canónica y el origen de la declaración. `cloneType` transporta ambos datos cuando vuelve a clonar
un argumento durante especializaciones anidadas. Finalmente,
`preserveOrAttachInstantiationOrigin` usa primero el proyecto/namespace declarador y conserva el
sitio consumidor únicamente como fallback para nombres que este pase temprano no pudo resolver.

`collectTemplates` records the declaration namespace. `processTypeSyntax` stores the canonical
key and declaration origin together. `cloneType` carries both when an argument is cloned again
during nested specialization. Finally, `preserveOrAttachInstantiationOrigin` prefers the
declaration project/namespace and retains the consumer request site only as a fallback for names
this early pass could not resolve.

La escritura de `baseName()` no cambia. Esto es obligatorio para que una especialización anidada
y una escrita directamente compartan la misma clave; reescribir `Payload` como nombre calificado
volvería a crear monomorfizaciones incompatibles del mismo tipo.

The `baseName()` spelling remains unchanged. This is required so a nested specialization and a
directly written one share the same key; rewriting `Payload` as a qualified name would recreate
incompatible monomorphizations of the same type.

### `include/nativesharp/AST/TypeSyntax.h`

Aclara el contrato de `originProjectOrdinal` y `originNamespace`: para argumentos resueltos son el
origen de la declaración; el sitio de petición es sólo el fallback conservador. Los campos siguen
vacíos en sintaxis ordinaria y sólo viajan en clones sustituidos.

Clarifies the `originProjectOrdinal` and `originNamespace` contract: for resolved arguments they
name the declaration origin; the request site is only the conservative fallback. The fields
remain empty on ordinary syntax and travel only on substituted clones.

### `tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp`

`testFirstExternalGenericSpecializationKeepsArgumentDeclarationOrigin` construye cuatro proyectos:
la biblioteca genérica, la biblioteca de tipos, un puente y el consumidor. El consumidor es el
primero que crea `Dictionary<string, T>` para una clase, un struct, una interfaz y un tipo anidado;
la biblioteca de tipos sólo es visible mediante una referencia transitiva.

`testFirstExternalGenericSpecializationKeepsArgumentDeclarationOrigin` builds four projects: the
generic library, the type library, a bridge, and the consumer. The consumer is the first project
to create `Dictionary<string, T>` for a class, struct, interface, and nested type; the type library
is visible only through a transitive reference.

## Por qué esta implementación / Why this implementation

La declaración resuelta es la fuente de verdad. Volver a resolver sólo por texto dentro del clon
es incorrecto porque el clon pertenece físicamente a la plantilla, no al proyecto que aportó el
argumento. Tampoco se cambia la visibilidad: `resolveDeclaredTypeIdentity` sigue exigiendo que la
declaración sea pública y alcanzable desde el proyecto que solicita la especialización.

The resolved declaration is authoritative. Resolving by text again inside the clone is incorrect
because the clone physically belongs to the template rather than the project supplying the
argument. Visibility is not widened: `resolveDeclaredTypeIdentity` still requires the declaration
to be public and reachable from the requesting project.

Separar identidad y origen evita dos errores opuestos: compartir tipos homónimos de módulos
distintos o duplicar el mismo tipo por diferencias de escritura. El ordinal de proyecto sólo vive
durante esta compilación; las claves persistentes siguen usando nombres estables de módulo.

Separating identity and origin avoids two opposite errors: sharing homonymous types from distinct
modules or duplicating one type because of spelling differences. The project ordinal only lives
during this compilation; persistent keys continue to use stable module names.

## Evidencia enfocada / Focused evidence

- antes del arreglo: Stage52 `139/141`; el nuevo caso falla al bajar a OOP MIR;
- después del arreglo: Stage52 `147/147`;
- Stage44 monomorfización: `75/75`;
- project name lookup: `76/76`;
- repro oficial de Saru `p10_stdlib_list_cross` compilado con el `nsc.exe` construido desde el
  disco actual;
- el ejecutable del repro imprime `p10` y devuelve `0`;
- compilador instalado en `C:\native-sharp` desde el contenido actual del disco:
  `0.9.0+build.20260909T191136Z.a7f5a44b95c61eb9`;
- el repro oficial se repitió con ese binario instalado, no sólo con el ejecutable del árbol de
  build;
- no se ejecutó el baseline completo para este arreglo urgente.

## Pendientes declarados / Declared follow-ups

- Saru puede retirar `NSC-LIMIT-023` y reevaluar sus caches tipados; el probe oficial ya está
  verde con el compilador instalado, pero la compilación global del port pertenece a su propio
  gate;
- el lote PDB extremo pausado continúa en el siguiente commit, sin mezclar esta corrección
  semántica con metadatos de depuración.

## Commit de referencia / Reference commit

**Título ES:** Conserva el origen declarador en especializaciones externas

**Title EN:** Preserve declaration origins in external specializations

**Descripción ES:** Propaga el proyecto y namespace de la declaración canónica al clonar
argumentos genéricos externos, permite que un consumidor cree la primera
`Dictionary<string, T>` sobre tipos de otra biblioteca y agrega cobertura de clases, structs,
interfaces, tipos anidados y referencias transitivas.

**Description EN:** Propagates the canonical declaration project and namespace when cloning
external generic arguments, allows a consumer to create the first `Dictionary<string, T>` over
types from another library, and adds coverage for classes, structs, interfaces, nested types, and
transitive references.
