# T08-002: resultados del segundo conjunto de identidad C

Ejecución dirigida en la rama `interop-t08-claude-identidad` con el build aislado
`A:\nsc-devel\build-claude-t08\build-ON` (Clang `A:\al82dev-clang` 23.1.0; `nsc`
del primer build completo de este carril, hash `2aea3fefe98e046b`;
`nsc_cpp_c_identity_tests` reconstruido incrementalmente sin advertencias;
`nslink.exe` existente de `A:\nsc-devel\build-clang\bin`, solo lectura).
Comando: `pruebas-tanda-08-identidad-c\validar.cmd` con `NSC_T08_BUILD`,
`NSC_T08_OUT=A:\nsc-devel\build-claude-t08\out\identidad-c`, `NSC_T08_CLANG` y
`NSC_LINKER` apuntando a ese build. Código de salida: 0.

No se ejecutó la batería de la tanda 08 ni otras suites: no es validación
integral.

## Encabezados homónimos (sonda 5 y fixture `homonimos`)

```text
USR homonym inline-function dirA=c:same.h@F@c_same_inline dirB=c:same.h@F@c_same_inline dirC=c:same.h@F@c_same_inline
USR homonym static-global dirA=c:same.h@c_same_static dirB=c:same.h@c_same_static dirC=c:same.h@c_same_static
USR homonym alias dirA=c:same.h@T@CSameInt dirB=c:same.h@T@CSameInt dirC=c:same.h@T@CSameInt
OBSERVATION typedef CSameInt from dirA/same.h and dirB/same.h: bound=yes diagnostics= error=''
OBSERVATION typedef CSameInt from dirA/same.h and dirC/same.h: bound=no diagnostics=[conflicting C/C++ type alias 'CSameInt'] error=''
Homonymous headers AliasAB compiled and ran.
Homonymous headers InlineA compiled and ran.
Homonymous headers AliasAC rejected: conflicting C/C++ type alias 'CSameInt'
Homonymous headers InlineAB rejected: ambiguous namespace function call to 'c_same_inline'
Homonymous headers InlineAC rejected: ambiguous namespace function call to 'c_same_inline'
```

La comprobación de que las dos funciones `static inline` con el mismo USR quedan
como dos grupos pasó. Observación previa al fixture, no versionada: un proyecto
que lee `c_same_static` falla con `unknown identifier` también con un solo
encabezado; NSC no importa globales `static`.

## Restauración por el módulo de la prueba y por el driver

Cada una de las veinte combinaciones (`Producer`, `ProducerValue`,
`ProducerUnion`, `ProducerAnonRecord` y `ProducerAnonKind` en O0–O3) terminó con:

```text
Consumer restores Clang USRs from the real producer NSMETA
Driver consumer restores Clang USRs from the real producer library
Restored C USRs passed <productor> O<n>.
```

USR restaurados nuevos, iguales en las ocho ejecuciones de cada productor:

```text
RESTORED record CAnonRecord usr=c:@SA@CAnonRecord
RESTORED field CAnonRecord.raw usr=c:@SA@CAnonRecord@FI@raw
RESTORED enum CAnonKind usr=c:@EA@CAnonKind
RESTORED enumerator CAnonKindFirst usr=c:@EA@CAnonKind@CAnonKindFirst
RESTORED enumerator CAnonKindSecond usr=c:@EA@CAnonKind@CAnonKindSecond
```

Los records, campos, enums y enumeradores de `Producer`, `ProducerValue` y
`ProducerUnion` coinciden con los del primer registro de resultados, ahora
también por la ruta del driver.

## Observaciones que no fallan el validador

```text
OBSERVATION anonymous enum field producer failed:
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification failed: C enum field dependency has no importable identity
Template producer: consumer of non-template C uses compiled and ran.
OBSERVATION ConsumerGeneric failed:
...CGenProducer.nsmeta.public-templates.ns:5:50: error: unknown identifier 'gen_c_counter' in callable/initializer context
...CGenProducer.nsmeta.public-templates.ns:12:36: error: unknown identifier 'gen_c_number' in callable/initializer context
OBSERVATION ConsumerGenericHeader failed:
(mismos dos errores)
OBSERVATION ConsumerAlias failed:
...CGenProducer.nsmeta.public-templates.ns:5:41: error: unknown type name 'GenCount' in module 'CGenProducer'
T08-002 C identity checks passed.
```

El enum anónimo usado como tipo de campo es el hallazgo H4. Las plantillas son la
evidencia de `T08-002-PROPUESTA-METADATA-C.md`. T08-002 permanece abierto.
