# Aritmética de campo en lifecycle binario

Una clase precompilada con constructor `Value = 41; Value = Value + 1` y
destructor `Value = Value - 1` se rechazaba como carga de Rc y Arc con
`shared payload class has no verifiable precompiled lifecycle body`. El
rechazo se reprodujo en O0/O2 (`shared-binary-arithmetic-before.log`). El MIR
del constructor y destructor está en
`A:/nsc-fixes-evidencia/shared-binary-arithmetic-ir/mir.log`.

El primer ajuste del productor reconocía únicamente el store de un campo `int`
de `this` cuyo valor es una suma o resta del mismo campo y un literal `int`.
Se comprueban el campo, el receptor y todos los hijos de esa forma antes de
acreditarla. El análisis sigue rechazando operaciones desconocidas: no se
presume seguro un cuerpo ausente ni se modifica el formato de metadata.

Tras la compilación incremental, las pruebas dirigidas con productor como
biblioteca estática y como DLL NativeSharp pasaron para Rc y Arc en O0/O2.
La carga devolvió el valor esperado, se destruyó una vez y expiró el débil.
En las mismas configuraciones se rechazaron la publicación de `this`, una
llamada opaca y una operación que lee otro campo. Los comandos y resultados
son `A:/nsc-fixes-evidencia/shared-binary-arithmetic-after.cmd` y
`shared-binary-arithmetic-dll.cmd`, con sus logs homónimos. También pasó el
control unitario dirigido de metadata (`shared-binary-arithmetic-unit.log`).

El productor se compiló en O0, la DLL permaneció cargada y Core binario se
reutilizó; no se acredita descarga dinámica, otras operaciones ni todos los
cuerpos de lifecycle. La prueba se ejecutó con otros cambios locales presentes.
`src/Interop/LibraryMetadataConstruction.cpp` es compartido y puede requerir
reconciliación con interoperabilidad; el cambio acredita metadata NativeSharp
y no modifica funcionalidad de interoperabilidad C/C++.

## Expresiones con campos propios

El control que antes se conservaba como rechazo de `Other + 1` se convirtió
en reproducción positiva; se añadió otra clase con `(First + Second) * 2 - 2`.
Ambas se rechazaron para Rc y Arc en O0/O2 antes del cambio
(`shared-binary-expressions-before.log`). El productor ahora recorre con una
pila explícita las expresiones `int` formadas por literales, lecturas directas
de campos `int` de `this`, y suma, resta o multiplicación. La escritura debe
seguir siendo sobre un campo `int` directo de `this`; cualquier nodo distinto
mantiene el resultado desconocido. La comprobación no acredita llamadas,
lecturas de otros objetos ni efectos globales.

La compilación incremental de `nsc` pasó. Con productores NativeSharp como
biblioteca estática y DLL, las dos cargas ejecutaron correctamente para Rc y
Arc en O0/O2, con un destructor observado y el débil expirado. Los controles
de publicación de `this`, llamada opaca y lectura de un campo de otro objeto
se rechazaron (`shared-binary-expressions-after.log` y
`shared-binary-expressions-dll.log`). El productor se compiló en O0, se
reutilizó Core binario y la DLL permaneció cargada. No se acreditan otras
formas de expresión o descarga dinámica.

La prueba se amplió para verificar la metadata recién producida mediante
`--verify-library-metadata` antes de compilar los consumidores. Esa
verificación y la selección funcional pasaron para biblioteca estática y DLL
(`shared-binary-expressions-verified.log` y
`shared-binary-expressions-verified-dll.log`). Esta verificación no sustituye
el control unitario que no pudo construirse.

El control unitario de metadata no llegó a ejecutarse: al compilar su objetivo,
Ninja informó que falta `C:/Program Files/Microsoft Visual Studio/18/Community/`
`DIA SDK/lib/amd64/diaguids.lib` (`shared-binary-expressions-unit-build.log`).
La existencia de esa ruta se comprobó y el archivo no está presente. Los
resultados anteriores del control unitario corresponden al código anterior;
la regresión funcional dirigida sí terminó correctamente.
