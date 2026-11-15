# T09-003: uniones C por valor con convenciones no `cdecl`

## Defecto

Sonda `build-claude-t08/tools/sonda-union.py`, evidencia
`A:/temp/nsc-t09-sonda-union-yspkssv0`. Casos probados: uniones de 4, 8, 12 y 24 bytes,
enteras, mixtas y solo `float`; en parámetro, retorno e ida y vuelta; con seis
convenciones.

| Convención | Directa | Por puntero |
|---|---|---|
| `cdecl`, `preserve_most`, `preserve_all` | pasan | pasan |
| `regcall` | pasan, salvo la unión solo `float` | pasan, salvo la unión solo `float` |
| `sysv_abi` | pasan | **se rechazan todas** |
| `preserve_none` | **4 de 6 rechazadas** | pasan |

`sysv_abi` por puntero falla con `record plan is not supported`, `record provenance is
invalid`, `scalar plan is not supported` y otros diagnósticos. En `preserve_none` directa
el diagnóstico es `preserve_none does not yet support this signature`.

Causa en el código. Las rutas solo admitían structs C donde los planes de Clang ya tratan
igual las uniones:

- El emisor de punteros C `sysv_abi` por plan (`OopLlvmClosuresDelegatesEvents.cpp`,
  `cSysVRecord`) exigía `CStruct`.
- La conciliación HIR de `preserve_none` directa (`recordPointerMatches`) solo admitía
  `CStruct` por valor.
- El replay precompilado de punteros `regcall`/`sysv_abi` (`CppProjectSessions.cpp`)
  reproducía solo `struct`.
- El plan precompilado de punteros C `preserve_none` admitía la unión solo apuntada.

## Cambio

- `cSysVRecord` acepta `CUnion` con las mismas condiciones de layout. La unión se baja pieza
  por pieza según el plan de Clang del alias, igual que un struct.
- `recordPointerMatches` (HIR) acepta una unión C por valor. Como un struct C, siempre es
  trivialmente copiable.
- Replay precompilado `regcall`/`sysv_abi`: una unión por valor se reproduce con su propio
  tipo (`union __nsc_abi_record…`), se exige la procedencia `CUnion` y se comparan el tipo
  del catálogo y cada campo con `matchesSimpleCRecordAbiReplay`.
- `planPrecompiledCPreserveNonePointer` acepta uniones C por valor y la selección de ruta las
  reconoce como records C.

No se modificó Clang ni NSLink. Los planes viajan en los campos existentes; el contrato
NSMeta vigente no cambia.

## Evidencia

### Sonda tras el cambio

`A:/temp/nsc-t09-sonda-union-by1p80sl`. Todas las celdas pasan en directa y por puntero,
salvo la unión solo `float` con `regcall`.

### Fixture nuevo

`pruebas-tanda-09-c-union-convenciones/validar.py`, con y sin `--dll`, siempre con
`--precompilado`. Cubre `regcall`, `sysv_abi`, `preserve_most`, `preserve_all` y
`preserve_none`, de O0 a O3, con estas uniones:

- `T09U4 {int; float}` con un `int`;
- `T09U8 {long long; double}` de ida y vuelta;
- `T09U12 {int[3]; float}` con un `double`;
- `T09UF {float; float}` con un `float`, salvo en `regcall`;
- `T09U24 {long long[3]; double}`, en parámetro y como retorno indirecto con un struct.

La prueba:

- compara con un control C compilado por Clang 23.1.0;
- consume en directo y por puntero, y desde una plantilla precompilada con y sin
  encabezado, con llamadas directas y por puntero;
- exige en el IR de NSC la convención y las extensiones de cada parámetro y del retorno
  idénticas a la definición de Clang, y la convención en todas las llamadas indirectas;
- exige que la unión original no cambie.

El control negativo comprueba que la unión solo `float` con `regcall`, que Clang pasa
expandida como `float`, sigue rechazada con su diagnóstico.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado | pasa (5 convenciones × O0–O3) | `A:/temp/nsc-t09-c-union-convenciones-myed7n4w` |
| DLL + precompilado | pasa (5 convenciones × O0–O3) | `A:/temp/nsc-t09-c-union-convenciones-a7zfev0r` |

### Control negativo convertido

`pruebas-tanda-09-sysv-indirecta/validar.py` usaba como rechazo `byval` una unión C de
24 bytes por un puntero `sysv_abi`, que ahora se baja por plan. Siguiendo la política
acordada:

- Esa unión se compara ahora con Clang: control C con `byval(%union…)` y llamada
  NativeSharp `byval` con resultado y argumento intactos.
- El rechazo se conserva con un callback C `sysv_abi` que recibe `__int128`. Clang lo
  compila y NSC no lo importa: `C callback signature contains an unavailable type`.

El mensaje anterior, `sysv_abi function pointer byval plan is not supported`, pertenece a
la ruta de records C++, a la que ya no llegan los records C. Evidencia:
`A:/temp/nsc-t09-sysv-indirecta-8rw2jwgl`.

## Límites

- La unión solo `float` con `regcall` se rechazaba, directa y por puntero. Clang la
  expande como `float`, y las rutas `regcall` expandidas solo conocían structs. Resuelto
  después; ver [t09-regcall-agregados-homogeneos.md](t09-regcall-agregados-homogeneos.md).
- El replay precompilado sigue sin campos `volatile`, `const` ni `_Alignas`. Los callbacks
  C con `__int128` no se importan.
