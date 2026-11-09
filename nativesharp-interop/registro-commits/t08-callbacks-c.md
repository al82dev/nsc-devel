# T08-017: punteros a función C

Fecha: 2026-11-08. Casillas: T08-017-02a, 03a–c, 04a–e, 05a–c, 06 y 07.

## Resultado

- **Corregido:** T08-017-02a, el conflicto `RecordCallback`.
- **Verificadas:** 03a–c, 04a–e, 05a–c, 06 y 07.
- **Pendiente:** T08-017-03d, `__vectorcall` no soportado (T08-017-03d1).
- **Decisión pendiente nueva:** T08-017-04a1.

## Corrección: typedef de puntero a función con record entre proyectos (02a)

**Síntoma.** `pruebas-tanda-08-callback/validar-record-precompilado.cmd` fallaba:

> conflicting C/C++ type alias 'RecordCallback' (projects 1,0; types 'int (*)(struct CallbackPayload *)', … ;
> reconciled=no)

Los tipos y los USR eran idénticos.

**Causa.** `compatibleFunctionAlias` usa `cppAliasFunctionType`, que no proyecta parámetros record. La
equivalencia `equivalentRecordFunctionAliases` de `CppSemanticInventory` solo compara records de catálogos
precompilados, así que no cubre el caso de un proyecto vivo (con encabezado) frente a uno precompilado.

**Corrección.** `QualifiedTypeNames.cpp` gana `reconciledFunctionRecordAlias`. Acepta el mismo typedef de
puntero a función entre dos proyectos si se cumplen todas estas condiciones:

- mismo USR y misma ortografía canónica;
- cada record de su firma (parámetros y resultado) es un record vivo ya conciliado
  (`reconciledIdentityKey`) con el record precompilado del otro proyecto.

Un typedef incompatible sigue en conflicto. Lo fija `typedef-funcion-record/validar.cmd --incompatible`, en la
regresión.

## Prueba

Nueva: `pruebas-tanda-08-callbacks-c/validar-callbacks.py`. Aprobada en O0–O3, con los rechazos en O0.

El cuerpo se ejecuta en un control Clang, en el camino directo, y en una plantilla precompilada consumida con
encabezado (05b) y sin él (05a, 05c).

- **03a:** callback escalar llamado desde NativeSharp y pasado a C.
- **03b:** callback con `Item *` y enum en la firma.
- **03c:** callback variádico. Su valor sobrevive a variables NativeSharp y C lo llama con argumentos
  variables.
- **04a:** callbacks en campos de struct C.
- **04b:** callbacks en globales C (`BinOp` y `volatile BinOp`), leídos, llamados y reasignados.
- **04c:** callbacks devueltos por funciones C, incluida la llamada directa sobre el resultado (`pick(0)(3, 4)`).
- **04d:** campo `volatile BinOp` alineado, y otro dentro de un struct `pack(1)`. Se leen, se llaman y se
  escriben.
- **04e:** escribir un campo `const BinOp` se rechaza: «not assignable».
- **Firma incompatible:** asignar `item_fn` a un `BinOp` se rechaza: «no method in the method group exactly
  matches delegate signature».

## T08-017-06 y T08-017-07: conciliación de notas históricas

Las notas antiguas sobre callbacks volátiles almacenados y sobre el método precompilado que devuelve un callback
quedan sustituidas por el comportamiento que fijan las pruebas actuales:

- 04b y 04d: lectura, llamada y escritura de callbacks `volatile` en global, campo alineado y campo empaquetado.
- 05a–c: un método precompilado que devuelve callbacks, con y sin encabezado
  (`validar-record-precompilado.cmd` incluido).

Las pruebas históricas no se modificaron.

## Pendiente T08-017-03d1

Un callback `__vectorcall` no está soportado. NSC lo rechaza con el diagnóstico explícito «C/C++ calling
convention 'vectorcall' is not yet supported in HIR for …vec_add@@16», que la prueba fija. Implementarlo es una
característica nueva: la convención `x86_vectorcallcc`, en HIR y en la bajada.

## Decisión pendiente T08-017-04a1

Una función C asignada desde NativeSharp a un puntero a función (`s.op = mul`) se guarda como el envoltorio
`mul__c_boundary`, un `invoke` con `__C_specific_handler`. La llamada funciona, pero la dirección no es la de
`mul`, así que una comparación en C (`s->op == mul`) da falso. La prueba comprueba esos campos llamándolos. La
sonda está en `A:\nsc-interop-build\claude\sondas\openarr\sonda26.py`.

**Opciones.**

1. Usar el símbolo C directamente cuando la función es C. Conserva la identidad, como en C, y omite la frontera.
2. Mantener la frontera, que contiene excepciones, a cambio de la identidad.
