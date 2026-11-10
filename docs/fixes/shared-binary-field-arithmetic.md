# Aritmética de campo en lifecycle binario

Una clase precompilada con constructor `Value = 41; Value = Value + 1` y
destructor `Value = Value - 1` se rechazaba como carga de Rc y Arc con
`shared payload class has no verifiable precompiled lifecycle body`. El
rechazo se reprodujo en O0/O2 (`shared-binary-arithmetic-before.log`). El MIR
del constructor y destructor está en
`A:/nsc-fixes-evidencia/shared-binary-arithmetic-ir/mir.log`.

El productor de metadata reconoce ahora únicamente el store de un campo `int`
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
