# Capturas de lambdas en funciones de namespace

Una lambda con captura creada en una función de namespace fallaba durante la
generación HIR porque el propietario se admitía solo para lambdas sin
capturas. El creador debe buscarse en el índice de funciones de namespace,
manteniendo la validación de ordinales. Después se reprodujo un error de
emisión con `-g`: la lambda y su helper de destrucción intentaban obtener el
propietario como miembro de clase. Se utiliza el rango del cuerpo y el nombre
generado para esa variante sin cambiar los ordinales de funciones.

La prueba `nsc.compiler.namespace-lambda-capture` pasó en O0/O2 y con `-g`
para capturas por copia, referencia y movimiento, incluidas varias lambdas y
funciones, además de controles sin captura y de clase. La prueba de FnOnce de
namespace pasó junto con las cinco regresiones relacionadas y dos de préstamos.
Los logs están en `A:/nsc-fixes-evidencia/namespace-lambda-integrated-20261111.log`,
`namespace-fnonce-integrated-20261111.log` y `namespace-lambda-borrow-20261111.log`.
Los casos inseguros solo se analizaron. El log de la primera ejecución fallida
se sobrescribió accidentalmente al repetir; esa limitación se documentó en
`A:/gaps-limitaciones/DEBUGGER-EMISION-CLOSURE-NAMESPACE.md`.

El control de dos archivos `namespace_lambda_debug_metadata.py` generó un
objeto con `-g` y `llvm-readobj --codeview` encontró el nombre de archivo y la
captura esperados (`namespace-lambda-codeview-20261111.log`). El compilador
tenía otros cambios locales y el control no prueba interacción con un depurador
ni todos los metadatos. `nslink.exe` se usó por nombre como dependencia externa
de solo lectura; ver `procedencia-herramientas-y-evidencia.md`.
