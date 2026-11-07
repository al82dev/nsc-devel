# T08-002: resultados de identidad C

Ejecución dirigida en la rama `interop-t08-claude-identidad` sobre el build
aislado `A:\nsc-devel\build-claude-t08\build-ON` (Clang `A:\al82dev-clang`
23.1.0, revisión fuente `ea7d852a`; compilador NSC con hash de build
`2aea3fefe98e046b`; `nslink.exe` existente de `A:\nsc-devel\build-clang\bin`, solo
lectura). Primer build completo de `nsc` y `nsc_cpp_session_tests`; después solo
compilación incremental de `nsc_cpp_c_identity_tests`, que terminó sin
advertencias. Comando: `pruebas-tanda-08-identidad-c\validar.cmd` con
`NSC_T08_BUILD`, `NSC_T08_OUT=A:\nsc-devel\build-claude-t08\out\identidad-c`,
`NSC_T08_CLANG` y `NSC_LINKER` apuntando a ese build. Código de salida: 0.

No se ejecutó la batería de la tanda 08, `nsc_cpp_session_tests` ni el resto de
las pruebas: esto no es validación integral.

## Sonda 1: USR observados (a.h, otra sesión de a.h, b.h)

```text
USR alias a.h=c:a.h@T@CIdInt a.h(again)=c:a.h@T@CIdInt b.h=c:b.h@T@CIdInt
USR anonymous-enumerator a.h=c:@Ea@CIdLoose@CIdLoose a.h(again)=c:@Ea@CIdLoose@CIdLoose b.h=c:@Ea@CIdLoose@CIdLoose
USR enum a.h=c:@E@CIdKind a.h(again)=c:@E@CIdKind b.h=c:@E@CIdKind
USR enumerator a.h=c:@E@CIdKind@CIdFirst a.h(again)=c:@E@CIdKind@CIdFirst b.h=c:@E@CIdKind@CIdFirst
USR field a.h=c:@S@CIdRecord@FI@value a.h(again)=c:@S@CIdRecord@FI@value b.h=c:@S@CIdRecord@FI@value
USR function a.h=c:@F@cid_function a.h(again)=c:@F@cid_function b.h=c:@F@cid_function
USR global a.h=c:@cid_global a.h(again)=c:@cid_global b.h=c:@cid_global
USR inline-function a.h=c:a.h@F@cid_inline a.h(again)=c:a.h@F@cid_inline b.h=c:b.h@F@cid_inline
USR record a.h=c:@S@CIdRecord a.h(again)=c:@S@CIdRecord b.h=c:@S@CIdRecord
USR static-global a.h=c:a.h@cid_static_global a.h(again)=c:a.h@cid_static_global b.h=c:b.h@cid_static_global
USR typedef-anonymous-record a.h=c:@SA@CIdAnonRecord a.h(again)=c:@SA@CIdAnonRecord b.h=c:@SA@CIdAnonRecord
USR union a.h=c:@U@CIdUnion a.h(again)=c:@U@CIdUnion b.h=c:@U@CIdUnion
```

Todas las categorías tienen USR y es igual entre sesiones independientes del
mismo encabezado. Typedef, función `static inline` y global `static` dependen
del nombre del encabezado; las demás no.

## Sondas 2 a 4

```text
OBSERVATION static inline cid_inline from a.h and b.h: 2 group(s)
OBSERVATION static cid_static_global from a.h and b.h: candidate=no conflict=''
OBSERVATION typedef CIdInt from a.h and a.h: bound=yes diagnostics= error=''
OBSERVATION typedef CIdInt from a.h and b.h: bound=no diagnostics=[conflicting C/C++ type alias 'CIdInt'] error=''
OBSERVATION anonymous CIdShared from c.h (1) and d.h (2): members=yes states=[value=0 importError='conflicting C enumerator declarations'] diagnostics= error=''
C declaration identity probes passed
```

Las comprobaciones de la sonda 2 (función y global externas, struct y enum con
etiqueta declarados en dos encabezados) pasaron. La línea `a.h and b.h` del
typedef es el hallazgo H1 de la auditoría.

## Restauración desde NSMETA real

En O0, O1, O2 y O3, para cada productor:

```text
Producer:      RESTORED record CRecord usr=c:@S@CRecord
               RESTORED field CRecord.value usr=c:@S@CRecord@FI@value
               RESTORED field CRecord.kind usr=c:@S@CRecord@FI@kind
               RESTORED enum CGlobalKind usr=c:@E@CGlobalKind
               RESTORED enumerator CGlobalChoice usr=c:@E@CGlobalKind@CGlobalChoice
ProducerValue: lo anterior más
               RESTORED record CBig usr=c:@S@CBig
               RESTORED field CBig.first usr=c:@S@CBig@FI@first
               RESTORED field CBig.second usr=c:@S@CBig@FI@second
               RESTORED field CBig.third usr=c:@S@CBig@FI@third
ProducerUnion: RESTORED record CUnionShared usr=c:@U@CUnionShared
               RESTORED field CUnionShared.value usr=c:@U@CUnionShared@FI@value
               RESTORED field CUnionShared.bytes usr=c:@U@CUnionShared@FI@bytes
```

Las doce combinaciones terminaron con `Consumer restores Clang USRs from the real
producer NSMETA` y el validador con `T08-002 C identity checks passed.`

Alcance: el NSMETA es el de un productor `nsc` real; el consumidor lo envuelve en
un módulo precompilado como el driver y no tiene encabezado C propio. No recorre
`preparePrecompiledProject`. No cubre funciones, globales ni alias, que no viajan
en el catálogo (hallazgo H3). T08-002 permanece abierto.
