# Limpieza de campos compartidos en constructor de biblioteca

La regresión `tests/integration/shared_binary_owner_constructor_failure.py`
comprueba la limpieza de un campo Rc, WeakRc, Arc o WeakArc cuando falla un
constructor de una clase compilada en un proyecto separado. La opción
`--dll` conserva la matriz existente y emite el productor como DLL, junto
con su biblioteca de importación y metadata; sin esa opción mantiene la
emisión estática.

Con el compilador aislado y Core binario preexistente de la campaña, las 32
combinaciones de la variante DLL devolvieron 42, igual que las 32 de la
variante estática. Se cruzan las cuatro familias, fallo en helper o constructor,
y productor y consumidor O0/O2. Los comandos y resultados están en
`A:/nsc-fixes-evidencia/shared-binary-owner-constructor-dll-current.cmd` y
`.log`, y `shared-binary-owner-constructor-static-current.cmd` y `.log`.

Las pruebas se ejecutaron con cambios locales adicionales en el compilador;
no certifican una revisión limpia. La DLL permaneció cargada y no se midió la
liberación física del bloque. La prueba no cubre descarga dinámica ni todos
los layouts o rutas de excepción. Se usaron herramientas y temporales
aislados en A, salvo las dependencias externas autorizadas.
