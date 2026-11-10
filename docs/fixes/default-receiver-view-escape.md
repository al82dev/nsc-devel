# Vistas de estructuras temporales por defecto

El análisis aceptaba una vista obtenida mediante `(default(Owner)).Borrow()`,
aunque esa vista podía referirse a un campo del receptor temporal. El
compilador ya rechazaba otras formas de receptor temporal; la corrección
incluye el valor por defecto de una estructura en esa misma comprobación.

La regresión `nsc.compiler.default-receiver-view-escape` verifica que se
rechacen tanto el retorno como la asignación local de esa vista. También
comprueba que un receptor proporcionado por `ref` devuelve 42 en O0 y O2.
Resultado: `A:/nsc-fixes-evidencia/default-receiver-focused-test.log`.

El compilador utilizado incluye cambios locales adicionales: la prueba no
certifica un árbol limpio. Los resultados compuestos con tuplas y otras formas
de receptor permanecen en el trabajo de vistas en curso. No se ejecutó una
batería completa ni se cambió interoperabilidad C/C++.
