# Destrucción de elementos de tupla completamente inicializada

En `cbedd8f04`, una tupla local completada que contenía `owner.Clone()` de `Rc`
o `Arc` terminaba su ámbito sin liberar el clon: su contador fuerte quedaba en 2
en vez de 1, tanto en O0 como O2. Las reproducciones y comandos se conservan en
`A:/nsc-fixes-evidencia/tuple-partial-cleanup-counts/`. La salida del programa es
10 más el contador. El LLVM conservado en
`A:/nsc-fixes-evidencia/tuple-partial-cleanup-probe/arc-aggregate-llvm.log`
muestra el bloque de destrucción que limpia la bandera sin destruir el elemento.

Las tuplas tienen layouts estructurales con índices de elementos, no ordinales de
miembro nominal. `emitValueDropAddress` pasaba esos índices a la ruta nominal y no
destruía los valores. La corrección recorre los elementos estructurales en orden
inverso, usa los offsets del layout existente y llama al drop de cada tipo que lo
necesita. Las estructuras nominales mantienen su ruta anterior.

La regresión dirigida `nsc.compiler.tuple-value-drop` ejecuta en Windows dos
valores con destructor y comprueba el orden 21, para literal, inicializador con
llaves y tupla anidada, en O0 y O2. Antes del cambio el literal O0 devolvía 0;
después los seis cruces devuelven 42. Registros:
`A:/nsc-fixes-evidencia/tuple-value-drop-before.log`, `tuple-value-drop-after.log`
y `tuple-value-drop-ctest.log`. En plataformas distintas de Windows la prueba solo
compila y no acredita la ejecución. Los tres controles dirigidos de agregados y
tuplas existentes pasaron (`tuple-owner-existing-controls.log`).

Los controles públicos desde fuentes para Rc y Arc muestran que, al salir
normalmente de la tupla, el contador pasa de 2 antes a 1 después en O0/O2
(`tuple-partial-cleanup-counts-after-drop/results.json`). Estos controles no
certifican el SDK binario ni todas las transferencias de propietarios.

La construcción interrumpida permanece pendiente: si la excepción sucede después
de uno o dos clones, los contadores siguen en 2 o 3 respectivamente. Esta
corrección no marca vivos elementos parcialmente inicializados ni define qué hacer
si un destructor lanza durante otra excepción. No se modificó interoperabilidad.
