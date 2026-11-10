# Carga compartida binaria con campo uint

Una clase NativeSharp precompilada con campo `uint`, constructor que escribe
`42u` y destructor que escribe `0u` se rechazaba al crear `Rc` o `Arc` con
`shared payload class has no verifiable precompiled lifecycle body`. La
reproducción de `shared-binary-unsigned-before.log` falló en ambas familias y
O0/O2, mientras la clase equivalente con campo `int` pasó. El MIR conservado
en `shared-binary-unsigned-ir/mir.log` muestra escrituras de constantes en el
campo directo de `this`; el tipo del campo no estaba en la lista escalar del
productor de la garantía de no escape.

La comprobación del productor reconoce ahora `builtin:uint` como campo escalar.
El cuerpo del constructor y destructor sigue sometido al mismo análisis
conservador; la ausencia de garantía sigue significando desconocido. El cambio
no modifica el formato de metadata ni la interoperabilidad C/C++.

La compilación incremental de `nsc` pasó. La prueba dirigida construyó un
productor NativeSharp en O0 como biblioteca estática y, por separado, como DLL;
para ambos, los consumidores con Rc y Arc ejecutaron en O0/O2 y observaron
`42u`, una destrucción y expiración del débil. Los controles con campo `uint`
que publican el receptor desde constructor o destructor se rechazaron en ambas
familias y niveles. Los casos y comandos están en
`A:/nsc-fixes-evidencia/shared-binary-unsigned-final.cmd` y
`shared-binary-unsigned-dll-final.cmd`, con logs homónimos. La prueba dirigida
de metadata `nsc.ns3.t12.exported-generic-ownership` pasó tras compilar su
objetivo incrementalmente (`shared-binary-unsigned-unit.log`).

La DLL permaneció cargada durante su uso; no se probó descarga del módulo con
punteros vivos. No se acreditan otros tipos de campo ni cuerpos más complejos.
El Core binario se reutilizó y el árbol contenía otros cambios locales; esto no
certifica una revisión limpia. El constructor de metadata es un archivo
compartido que puede entrar en conflicto con la rama de interoperabilidad.

## Otros campos enteros

Una clase binaria segura que reúne campos `sbyte`, `byte`, `char`, `short`,
`ushort`, `ulong`, `nint`, `nuint`, `i128` y `u128` también se rechazó para Rc
y Arc en O0/O2 (`shared-binary-integers-before.log`). El productor usa ahora el
catálogo canónico de enteros del frontend en lugar de una lista parcial; las
formas del cuerpo admitidas por el análisis de no escape no se ampliaron.

Tras la compilación incremental, los consumidores de biblioteca estática y
DLL leyeron los campos predeterminados, observaron una destrucción y la
expiración del débil para Rc y Arc en O0/O2. Las lecturas de 128 bits se
comprobaron mediante conversión a `long`, por lo que no acreditan sus bits
altos. Una clase con `nint` e `i128` que publica `this` se rechazó para ambas
familias y niveles; también pasaron los negativos anteriores. Los registros son
`shared-binary-integers-readback.log` y `shared-binary-integers-readback-dll.log`.
El control unitario de metadata pasó (`shared-binary-integers-unit.log`).
El productor de estas pruebas se compiló en O0; no se probó descarga dinámica
ni todos los cuerpos de lifecycle. El compilador contenía otros cambios locales
y se reutilizó Core binario, por lo que no se certifica una revisión limpia.

## Campo decimal

Una clase binaria con un campo `decimal` predeterminado también recibió el
rechazo de lifecycle no verificable para Rc y Arc en O0/O2
(`shared-binary-decimal-before.log`). El productor admite ahora ese valor
incorporado de 128 bits en la misma comprobación de campos; el análisis del
cuerpo conserva sus restricciones anteriores.

La regresión dirigida construyó el productor como biblioteca estática y DLL
NativeSharp, y sus consumidores Rc/Arc en O0/O2 leyeron el decimal cero,
observaron una destrucción y la expiración del débil. Un constructor con
campo decimal que publica `this` siguió rechazándose en ambas familias y
niveles. Los comandos y logs son `shared-binary-decimal-after` y
`shared-binary-decimal-dll` bajo `A:/nsc-fixes-evidencia`. Pasó el control
unitario de metadata (`shared-binary-decimal-unit.log`). El productor se
compiló en O0 y la DLL permaneció cargada; no se verificó descarga dinámica ni
la inicialización decimal explícita. El SDK binario se reutilizó y el árbol
contenía otros cambios locales.
