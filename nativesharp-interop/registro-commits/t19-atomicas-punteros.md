# T19-038: punteros a dato atómico y a dato `volatile` fuera de la dirección de una global

Fixture nuevo: `pruebas-tanda-19-atomicas-punteros/validar.py`, con y sin `--dll`. Sonda de desarrollo:
`build-claude-t08/tools/t19c/d18/sonda-t19-038.py` (`logs/sonda38.log`).

## Defectos

Confirmados con la sonda antes del cambio; seguían rechazados desde T19-002:

- Un campo C `_Atomic int*` no se importaba («has no compatible member named 'p'»).
- Una global C `_Atomic int*` tampoco («C/C++ global type is not yet mapped»).
- Un tipo de función C con `_Atomic int*` en un parámetro o en el resultado no se mapeaba. La función C que lo
  devuelve o lo recibe no se importaba («C/C++ return type is not yet mapped», «parameter 0 is not yet mapped»).
- `&a` de una global `_Atomic(const int*)` o `_Atomic(volatile int*)` se rechazaba («cannot take the address of C
  atomic storage holding a pointer to qualified data until nested qualified pointers are supported»).
- `&v` de una global `volatile` no atómica, escalar o record, se rechazaba («cannot take the address of a qualified
  C/C++ global until qualified pointers are supported»).

## Decisiones

- **Portador común**: `cAtomicPointeeCarrier` convierte un `_Atomic T*` en el puntero a T con una marca atómica.
  Antes solo lo usaban las funciones; ahora pasa a `src/Interop/Internal/CAtomicPointee.h`, con su comentario, y lo
  usan también globales, campos y tipos de función.
- **Globales y campos**: `CppSemanticGlobal::pointerPointeeAtomic` y `CppSemanticInstanceField::pointeeAtomic`
  llevan la marca hasta `NamespaceStateSymbol::cppPointerPointeeAtomic` y
  `ClassMemberSymbol::cppDeclaredPointerPointeeAtomic`.
  - La lectura de la global o del campo da un puntero a dato atómico, así que cada acceso por él es atómico, como el
    de un parámetro (T19-002).
  - `&gp` y `&h.p` darían un `_Atomic T**`, un calificador en un nivel intermedio que NativeSharp no representa.
    Siguen rechazados, con el diagnóstico de los punteros anidados.
- **Tipos de función**: la identidad estructural guarda un sufijo opcional `:atomic-pointee:`, con las mismas reglas
  que `const` y `volatile` (T08-002).
  - Va después de los otros, así que las identidades sin la marca conservan exactamente sus bytes.
  - Una llamada indirecta acepta un puntero a dato atómico solo en un parámetro `_Atomic T*`, y ese parámetro solo
    acepta un puntero así, como C.
  - El resultado conserva la marca.
  - Un método NativeSharp nunca coincide con un tipo así: no hay sintaxis para escribirlo. Una lambda para ese tipo
    recibe sus parámetros con la marca.
  - Un tipo con la marca no se materializa en la fuente de una interfaz precompilada (`MetadataSourceMapping`).
- **Global atómica con puntero calificado**: `&a_cp` es un puntero a almacenamiento atómico.
  - El `const` del tipo base sigue T08-D17, con `pointerPointeeReadonly` en un puntero a puntero.
  - El `volatile` va en `cppAtomicPointeeHoldsVolatile`. Ese indicador viaja como la marca atómica: locales `var`,
    aritmética de punteros, `?:` y `??`.
  - `*pp` es atómico, y el puntero leído conserva su calificador.
- **Global `volatile`**: `&v` de una global C `volatile` no atómica, escalar o record y también `const`, es un
  puntero a dato `volatile` (`cppVolatileGlobalAddress`).
  - Con un escalar inicia el contrato C de T08-D13: modo `volatile` de la global y alineación natural.
  - El almacenamiento sigue sin ser direccionable para `ref`, `in` y `out`.
  - Una global arreglo, o una cuyo valor es un puntero a dato calificado, no entra.
- **Catálogo precompilado**: el tipo de un campo de record puede ser `_Atomic T*`, con la misma validación que los
  tipos de dependencia. Los campos sin `_Atomic` conservan sus bytes, y el contrato NSMeta no cambia.

No se modificó Clang ni NSLink.

## Qué funciona, como en Clang

En C, en estática y DLL O0–O3, directo y desde una plantilla precompilada con y sin encabezado:

- leer `h.p` de una global record con un campo `_Atomic int*` y escribir por él; asignar el campo de un local `H`
  creado con `new` y de la global;
- leer, escribir y reasignar una global `_Atomic int*`;
- llamar por puntero un `typedef int (*AtomicCb)(_Atomic int*)` obtenido de C, con `&a_int`, `h.p` y `gp`, y pasarlo
  a otra función C;
- llamar un `typedef _Atomic int* (*GetCb)(void)` y escribir por el resultado;
- `&a_cp` de `_Atomic(const int*)` y `&a_vp` de `_Atomic(volatile int*)`: guardar en ellas, leerlas y usar el puntero
  leído (`*u += 1` volatile);
- `&vg` de `volatile int` y `&vr` de un `volatile R`: escribir por el puntero y pasarlo a un parámetro
  `volatile int*`.

En el IR de NSC hay 8 lecturas, 4 escrituras y 3 actualizaciones atómicas `seq_cst`, y 7 accesos `volatile`.
Resultado del control: 752566.

Rechazos genuinos, también en Clang:

- un `int*` desde un campo `_Atomic int*`;
- un `int*` a un parámetro `_Atomic int*` de un callback C;
- la escritura por el `const int*` leído de `_Atomic(const int*)`;
- convertir una función NativeSharp a un tipo de función C con `_Atomic T*`.

## Negativos obsoletos convertidos

`pruebas-tanda-08-globales-c` esperaba que `&g_vi` y `&g_cvi` se rechazaran. Ahora se comparan con Clang:

- `validar-direccion-const.py` lee y escribe por `&g_vi` y lee por `&g_cvi`, directo, en DLL y desde la plantilla
  precompilada sin encabezado, O0–O3. Sus rechazos genuinos, que Clang comparte con `-Werror`, son perder
  `volatile` (`int* p = &g_vi;`) y escribir por `&g_cvi`. La dirección de un puntero a `const` sigue rechazada.
- `validar-escalares-calificados.py` conserva el rechazo de perder `volatile` (`int* p = &g_vi;`).

Las dos suites aceptan ahora `NSC_T08_NSC`, `NSC_T08_CLANG`, `NSC_T08_RUNTIME`, `NSC_T08_NSLINK` y `NSC_T08_TEMP`,
como las convertidas en T19-002.

## Evidencia

| Fixture | Estática | DLL |
|---|---|---|
| `pruebas-tanda-19-atomicas-punteros` | `A:/temp/nsc-t19-atomicas-punteros-j1l0_2mp` | `A:/temp/nsc-t19-atomicas-punteros-qwy57jil` |

Validación puntual, por decisión del usuario (`build-claude-t08/tools/t19c/d18/p63.cmd`, `logs/p63.log`):

- DLL: el fixture nuevo y `pruebas-tanda-19-atomicas-direccion` pasan, 2 de 2 (`logs/regresion-dll-d63.log`).
- Las cinco suites de `pruebas-tanda-08-globales-c` tocadas por el cambio terminan con 0 con el NSC y el Clang
  actuales: `validar-direccion-const` (`A:/nsc-interop-build/t08-direccion-const-b97g8pm3`),
  `validar-escalares-calificados` (`A:/nsc-interop-build/t08-globales-escalares-i47ks2_8`), `validar-atomicas`,
  `validar-tls` y `validar-atomicas-record` (`logs/fx63-*.log`).
- Tanda 19 en estática: pasan los 18 guiones (`logs/regresion-t19-d63.log`), entre ellos el fixture nuevo
  (`A:/temp/nsc-t19-atomicas-punteros-o_ovwvxq`).

La regresión global queda para el final.

## Límites

- `&gp` de una global `_Atomic T*` y `&h.p` de un campo así darían un puntero a puntero con un calificador
  intermedio, que NativeSharp no representa. Siguen rechazados con diagnóstico, como cualquier `T* volatile*`.
- La dirección de una global C cuyo valor es un puntero a `const` o a `volatile` no atómica sigue rechazada por el
  mismo motivo (T08-D17).
