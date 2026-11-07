# T08-002: resultados del tercer conjunto de identidad C

Ejecución dirigida en la rama `interop-t08-claude-identidad` con el build aislado
`A:\nsc-devel\build-claude-t08\build-ON`:

- Clang `A:\al82dev-clang` 23.1.0.
- `nsc` del primer build completo de este carril, hash `2aea3fefe98e046b`, sin
  reconstruir.
- `nsc_cpp_c_identity_tests` reconstruido de forma incremental, sin advertencias.
- `nslink.exe` existente de `A:\nsc-devel\build-clang\bin`, solo lectura.

Comando: `pruebas-tanda-08-identidad-c\validar.cmd` con `NSC_T08_BUILD`,
`NSC_T08_OUT=A:\nsc-devel\build-claude-t08\out\identidad-c`, `NSC_T08_CLANG` y
`NSC_LINKER` apuntando a ese build, sin `NSC_T08_PROPUESTAS`. Código de salida:
0.

Solo se ejecutó el estado final de la rama. Los commits intermedios de este
conjunto no se ejecutaron por separado. No se ejecutó la batería de la tanda 08
ni otras suites: no es validación integral.

## Sondas 1–5 y restauración (sin cambios)

Las tablas de USR y las observaciones coinciden con
`t08-002-resultados-identidad-c-2.md`. Las veinte combinaciones de restauración
(cinco productores, O0–O3, módulo de la prueba y ruta del driver) terminaron con
`Restored C USRs passed`. Los cinco casos homónimos dieron el resultado
esperado.

## Sonda 6: enum dentro de un campo de record (afirmada)

```text
OBSERVATION enum snapshot lists nested enumerator CNestLow: no
OBSERVATION enum snapshot lists nested enumerator CNestNamedA: no
OBSERVATION nested enum CNestAnonField.level: name='' qualified='CNestAnonField::enum (unnamed at A:\nsc-devel\build-claude-t08\out\identidad-c\c-identity\nested-copy.h:1:27)' anonymousContext=yes usr=c:@SA@CNestAnonField@E@nested-copy.h@26 fieldUsr=c:@SA@CNestAnonField@FI@level underlying=int
OBSERVATION nested enum CNestAnonField.level: name='' qualified='CNestAnonField::enum (unnamed at A:\nsc-devel\build-claude-t08\out\identidad-c\c-identity\nested.h:1:27)' anonymousContext=yes usr=c:@SA@CNestAnonField@E@nested.h@26 fieldUsr=c:@SA@CNestAnonField@FI@level underlying=int
OBSERVATION nested enum CNestNamedField.kind: name='CNestNamed' qualified='CNestNamed' anonymousContext=no usr=c:@E@CNestNamed fieldUsr=c:@SA@CNestNamedField@FI@kind underlying=int
```

## Sonda 7: catálogo heredado solo de enums (observación)

```text
OBSERVATION legacy enum-only C catalogue encoded=yes first bytes=04 00 00 00 00 00 00 00 00 00 00 00 classified-as-envelope=yes decoded=no detail='invalid C record catalogue size'
OBSERVATION versioned enum-only C catalogue encoded=yes first bytes=01 00 00 00 f0 00 00 00 00 00 00 00 
```

## Matriz de typedef (`--typedef-matrix` y compilación)

```text
TYPEDEF T01Int (compatible) CTdInt
  x.h   c:x.h@T@CTdInt 'int' record=- enum=-
  y.h   c:y.h@T@CTdInt 'int' record=- enum=-
  p/t.h c:t.h@T@CTdInt 'int' record=- enum=-
  q/t.h c:t.h@T@CTdInt 'int' record=- enum=-
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T02IntLong (distinto) CTdWide
  x.h   c:x.h@T@CTdWide 'int' record=- enum=-
  y.h   c:y.h@T@CTdWide 'long long' record=- enum=-
  p/t.h c:t.h@T@CTdWide 'int' record=- enum=-
  q/t.h c:t.h@T@CTdWide 'long long' record=- enum=-
  current XY=reject PQ=reject; proposed XY=reject PQ=reject
TYPEDEF T03Spelling (compatible) CTdUnsigned
  x.h   c:x.h@T@CTdUnsigned 'unsigned int' record=- enum=-
  y.h   c:y.h@T@CTdUnsigned 'unsigned int' record=- enum=-
  p/t.h c:t.h@T@CTdUnsigned 'unsigned int' record=- enum=-
  q/t.h c:t.h@T@CTdUnsigned 'unsigned int' record=- enum=-
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T04Chain (compatible) CTdChain
  x.h   c:x.h@T@CTdChain 'int' record=- enum=-
  y.h   c:y.h@T@CTdChain 'int' record=- enum=-
  p/t.h c:t.h@T@CTdChain 'int' record=- enum=-
  q/t.h c:t.h@T@CTdChain 'int' record=- enum=-
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T05Struct (compatible) CTdStructAlias
  x.h   c:x.h@T@CTdStructAlias 'struct CTdS' record=c:@S@CTdS enum=-
  y.h   c:y.h@T@CTdStructAlias 'struct CTdS' record=c:@S@CTdS enum=-
  p/t.h c:t.h@T@CTdStructAlias 'struct CTdS' record=c:@S@CTdS enum=-
  q/t.h c:t.h@T@CTdStructAlias 'struct CTdS' record=c:@S@CTdS enum=-
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T06StructDiffers (distinto) CTdOtherAlias
  x.h   c:x.h@T@CTdOtherAlias 'struct CTdO' record=c:@S@CTdO enum=-
  y.h   c:y.h@T@CTdOtherAlias 'struct CTdO' record=c:@S@CTdO enum=-
  p/t.h c:t.h@T@CTdOtherAlias 'struct CTdO' record=c:@S@CTdO enum=-
  q/t.h c:t.h@T@CTdOtherAlias 'struct CTdO' record=c:@S@CTdO enum=-
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T07AnonStruct (compatible) CTdAnon
  x.h   c:@T@CTdAnon 'CTdAnon' record=c:@SA@CTdAnon enum=-
  y.h   c:@T@CTdAnon 'CTdAnon' record=c:@SA@CTdAnon enum=-
  p/t.h c:@T@CTdAnon 'CTdAnon' record=c:@SA@CTdAnon enum=-
  q/t.h c:@T@CTdAnon 'CTdAnon' record=c:@SA@CTdAnon enum=-
  current XY=accept PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T08Enum (compatible) CTdEnumAlias
  x.h   c:x.h@T@CTdEnumAlias 'enum CTdE' record=- enum=c:@E@CTdE
  y.h   c:y.h@T@CTdEnumAlias 'enum CTdE' record=- enum=c:@E@CTdE
  p/t.h c:t.h@T@CTdEnumAlias 'enum CTdE' record=- enum=c:@E@CTdE
  q/t.h c:t.h@T@CTdEnumAlias 'enum CTdE' record=- enum=c:@E@CTdE
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T09OtherTag (distinto) CTdTag
  x.h   c:x.h@T@CTdTag 'struct CTdP' record=c:@S@CTdP enum=-
  y.h   c:y.h@T@CTdTag 'struct CTdQ' record=c:@S@CTdQ enum=-
  p/t.h c:t.h@T@CTdTag 'struct CTdP' record=c:@S@CTdP enum=-
  q/t.h c:t.h@T@CTdTag 'struct CTdQ' record=c:@S@CTdQ enum=-
  current XY=reject PQ=reject; proposed XY=reject PQ=reject
TYPEDEF T10FunctionPointer (compatible) CTdFn
  x.h   c:x.h@T@CTdFn 'int (*)(int)' record=- enum=-
  y.h   c:y.h@T@CTdFn 'int (*)(int)' record=- enum=-
  p/t.h c:t.h@T@CTdFn 'int (*)(int)' record=- enum=-
  q/t.h c:t.h@T@CTdFn 'int (*)(int)' record=- enum=-
  current XY=reject PQ=accept; proposed XY=accept PQ=accept
TYPEDEF T11SameTagFields (compatible): no typedef under test
TYPEDEF T12OtherTagFields (distinto): no typedef under test
Typedef identity matrix passed
Case T01IntX1 compiled and ran.
Case T01IntXY rejected as alias: conflicting C/C++ type alias
Case T01IntPQ compiled and ran.
Case T02IntLongX1 compiled and ran.
Case T02IntLongXY rejected as alias: conflicting C/C++ type alias
Case T02IntLongPQ rejected as alias: conflicting C/C++ type alias
Case T03SpellingX1 compiled and ran.
Case T03SpellingXY rejected as alias: conflicting C/C++ type alias
Case T03SpellingPQ compiled and ran.
Case T04ChainX1 compiled and ran.
Case T04ChainXY rejected as alias: conflicting C/C++ type alias
Case T04ChainPQ compiled and ran.
Case T05StructX1 compiled and ran.
Case T05StructXY rejected as alias: conflicting C/C++ type alias
Case T05StructPQ compiled and ran.
Case T06StructDiffersX1 compiled and ran.
Case T06StructDiffersXY rejected as alias: conflicting C/C++ type alias
Case T06StructDiffersPQ rejected as record: referenced C/C++ record has conflicting or unverified definitions
Case T07AnonStructX1 compiled and ran.
Case T07AnonStructXY compiled and ran.
Case T07AnonStructPQ compiled and ran.
Case T08EnumX1 compiled and ran.
Case T08EnumXY rejected as alias: conflicting C/C++ type alias
Case T08EnumPQ compiled and ran.
Case T09OtherTagX1 compiled and ran.
Case T09OtherTagXY rejected as alias: conflicting C/C++ type alias
Case T09OtherTagPQ rejected as alias: conflicting C/C++ type alias
Case T10FunctionPointerX1 compiled and ran.
Case T10FunctionPointerXY rejected as alias: conflicting C/C++ type alias
Case T10FunctionPointerPQ compiled and ran.
Case T11SameTagFieldsX1 compiled and ran.
Case T11SameTagFieldsXY rejected as member: has no compatible member named
Case T11SameTagFieldsPQ rejected as member: has no compatible member named
Case T12OtherTagFieldsX1 compiled and ran.
Case T12OtherTagFieldsXY rejected as record: referenced C/C++ record has conflicting or unverified definitions
Case T12OtherTagFieldsPQ rejected as record: referenced C/C++ record has conflicting or unverified definitions
```

## Enum dentro de un campo (`enum-campo/casos.txt`)

```text
Case HRaw compiled and ran.
Case HLevel rejected as member: has no compatible member named
Case HLevelInt rejected as member: has no compatible member named
Case HConst rejected as ident: unknown identifier
Case HNamed rejected as member: has no compatible member named
Case HNamedType rejected as type: unknown type name
Case CAnon rejected as field-identity: C enum field dependency has no importable identity
Case CAnonHeader rejected as field-identity: C enum field dependency has no importable identity
Case CNamed rejected as enum-unavailable: required C enum is unavailable
Case CInternal rejected as member: has no compatible member named
```

## Plantillas y cuerpos de biblioteca (`plantillas/casos.txt`)

```text
Case ConsumerPlain compiled and ran.
Case ConsumerRecordParam compiled and ran.
Case ConsumerInlinePlain compiled and ran.
Case ConsumerGeneric rejected as ident: unknown identifier
Case ConsumerGenericHeader rejected as ident: unknown identifier
Case ConsumerAlias rejected as type: unknown type name
Case ConsumerRecordLocal rejected as ident: unknown identifier
Case ConsumerGlobalWrite rejected as ident: unknown identifier
Case ConsumerInline rejected as ident: unknown identifier
Case ConsumerEnumConstantPlain rejected as state: metadata contains an incomplete or duplicate namespace state
Case ConsumerEnumConstant rejected as state: metadata contains an incomplete or duplicate namespace state
Case ConsumerAnonymousConstantPlain rejected as state: metadata contains an incomplete or duplicate namespace state
Case ConsumerAnonymousConstant rejected as state: metadata contains an incomplete or duplicate namespace state
```

Primer diagnóstico de cada caso rechazado, tomado de los registros del
validador (`out\identidad-c\plantillas` y `out\identidad-c\enum-campo`; build
hash `2aea3fefe98e046b`):

```text
plantillas\CGenProducer.nsmeta.public-templates.ns:5:50: error: unknown identifier 'gen_c_counter' in callable/initializer context   (ConsumerGeneric)
plantillas\CGenProducer.nsmeta.public-templates.ns:5:41: error: unknown type name 'GenCount' in module 'CGenProducer'                (ConsumerAlias)
plantillas\CGenProducer.nsmeta.public-templates.ns:5:55: error: unknown identifier 'gen_c_record' in callable/initializer context    (ConsumerRecordLocal)
plantillas\CGenProducer.nsmeta.public-templates.ns:5:46: error: unknown identifier 'gen_c_counter' in callable/initializer context   (ConsumerGlobalWrite)
plantillas\CGenProducerInline.nsmeta.public-templates.ns:5:48: error: unknown identifier 'gen_c_twice' in callable/initializer context (ConsumerInline)
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification failed: metadata contains an incomplete or duplicate namespace state '.GenModeB'   (CGenProducerEnum)
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification failed: metadata contains an incomplete or duplicate namespace state '.GenLoose'   (CGenProducerLoosePlain)
enum-campo\HLevel.ns:1:43: error: receiver/type 'type:HLevel::CNestAnonField' has no compatible member named 'level'
enum-campo\HLevel.ns:1:55: error: unknown identifier 'CNestHigh' in callable/initializer context
nsc: error: record observation name lookup failed: unknown type name 'CNestNamed' in module 'HNamedType'
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification failed: C enum field dependency has no importable identity   (PAnon)
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification failed: required C enum is unavailable   (PNamed)
```

Resultado final del validador:

```text
T08-002 C identity checks passed.
```
