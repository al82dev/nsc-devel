# Limpieza de tuplas temporales descartadas

Una sentencia como `(owner.Clone(), 0);` terminaba sin destruir el clon que
contenía la tupla. La reproducción con `Rc`, `WeakRc`, `Arc` y `WeakArc`
compiló en O0/O2, pero las ocho ejecuciones observaron un contador incorrecto;
las ocho variantes que guardaban la tupla en una variable devolvieron el
resultado esperado. Entradas y salidas originales: `discard.log` en
`A:/nsc-fixes-evidencia/shared-tuple-call-commit-20261111/`.

La tupla se baja a `AggregateInitializer`. En `ownDroppableTemporaries`, los
resultados descartados se entregan a un dueño temporal sólo para productores
reconocidos por `isFreshDroppableTemporary`, que no incluye ese nodo. El cambio
reconoce localmente una tupla literal descartada cuyo tipo requiere destrucción
y reutiliza la limpieza de expresión completa. No cambia la clasificación
global de llamadas ni las tuplas cuyo resultado se consume. El backend ya
registra los elementos construidos parcialmente ante una excepción.

El build incremental del compilador aislado terminó correctamente. Con el
binario SHA-256 `3eab8df25f41db523a03e8cf31215ac5a54c78661b981e554928513f6a7726eb`,
el sondeo original pasó 16/16; la nueva prueba dirigida pasó 24/24: las cuatro
familias, descarte, fallo del segundo elemento y conservación en variable,
en O0/O2. La prueba incluye además una tupla escalar descartada. Pasaron las
dos pruebas dirigidas de transferencia de tuplas a métodos. Comandos y logs:
`discard-after.cmd`, `discard-after.log`, `discard-regressions.cmd`,
`discard-regressions.log`, `call-after.cmd` y `call-after.log`, en la carpeta
de evidencia indicada arriba. El control de construcción parcial mediante
biblioteca estática se repitió con el compilador actualizado y pasó las ocho
combinaciones de familia y optimización (`partial-after.cmd` y `.log`).
La prueba permanente registrada en CTest también pasó sin omisión mediante
`discard-ctest.cmd` (`discard-ctest.log`).

La compilación y pruebas usaron la copia de trabajo con otros cambios locales
y Core binario anterior; no demuestran el resultado sobre una revisión limpia
ni un Core generado con este compilador. No cubren `await`, DLL, tuplas
condicionales ni todos los patrones de excepciones. No se modifica
interoperabilidad C/C++, LLVM ni la instalación de C:;
se usan compilación incremental y pruebas dirigidas.
