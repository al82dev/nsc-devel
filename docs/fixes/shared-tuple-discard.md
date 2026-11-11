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

## Resultado condicional descartado

La corrección inicial no cubría `choose ? (first.Clone(), 0) :
(second.Clone(), 0)` como sentencia. La reproducción compiló, pero el contador
quedó incrementado para Rc y Arc, ambas ramas y O0/O2: ocho salidas 2 frente a
42. Los ocho controles con tupla literal anidada sí devolvieron 42. Entradas,
comandos y resultados: `conditional.cmd` y `conditional.log` en la carpeta de
evidencia anterior.

La unión se representa en HIR como `Conditional`, no `AggregateInitializer`.
El ensayo aplica el dueño temporal al resultado descartado únicamente cuando
su tipo es una tupla move-only que necesita drop; el análisis existente rechaza
las ramas addressable que intentarían copiar ese tipo. Se conserva la selección
de ramas y no se altera la clasificación global de otros temporales.

Con compilación incremental, el sondeo original pasó 16/16. La regresión nueva
pasó 48 combinaciones: cuatro familias, ambas ramas, resultado descartado,
excepción durante la construcción y resultado guardado, en O0/O2; también
rechazó copiar una tupla move-only existente. Los dos dueños tienen contadores
separados, lo que comprueba cuál rama adquirió una referencia. Pasaron de nuevo
el control anterior de descarte y las dos pruebas de paso a método. Resultados:
`conditional-after.log`, `conditional-regressions-final.log`,
`conditional-controls.log` y `conditional-call-controls.log` en la carpeta de
evidencia anterior. Estos resultados usan el árbol con otros cambios locales y
el Core binario anterior: no demuestran una revisión limpia ni acreditan
tuplas copiables con destructor, `await`, `match` o contratos binarios generales.
El compilador utilizado tiene SHA-256
`84b3406e935d6bd5a71007c34daeea1d2040f3bf868cf1341bda8676fef99ac1`;
la selección registrada de CTest pasó 1/1 sin omisión (`conditional-ctest.log`).
