# Resolución léxica de tipos genéricos

Al resolver un tipo genérico, el compilador buscaba por el nombre terminal sin
respetar los imports ni los alias del ámbito. Así aceptaba un tipo de un
namespace no importado y rechazaba un único import cuando existía un homónimo.
El contraste con tipos ordinarios y los diagnósticos iniciales están en
`A:/nsc-fixes-evidencia/generic-type-visibility-before.log` y
`generic-type-visibility-expanded.log` (este último se actualiza al repetir la
prueba). La reproducción del informe de matriz se conserva sin cambios.

El resolvedor usa ahora la escala de nombres compartida con el ámbito léxico
que reciben las cuatro rutas de demanda. Al procesar un clon de tipo genérico,
se restablecen los imports y alias de la declaración del productor; usar un
ámbito vacío rompía el Core binario porque sus plantillas dependen de tipos
internos importados. La búsqueda explícita por módulo no se modificó.

Con el compilador aislado en A se ejecutaron cuatro pruebas dirigidas: visibilidad
en un proyecto, visibilidad entre proyectos, ámbitos separados con cambio del
consumidor y caché de bundle, y productor/consumidor binario. Pasaron 4/4
(`A:/nsc-fixes-evidencia/generic-type-lexical-ctest.log`); los positivos de
visibilidad y el consumo binario se comprobaron en O0 y O2. También pasaron
los controles de reconstrucción binaria y las familias de punteros con Core
binario preexistente (`generic-type-lexical-binary.log` y
`generic-type-lexical-pointers.log`). El SDK binario no se regeneró.

El bundle caliente se comprobó con un hit y se forzó una nueva compilación al
cambiar el consumidor. Las cachés internas C1 y C3 del modelo genérico siguen
desconectadas en la compilación de proyectos; sus rutas de restauración no se
han ejecutado. No se demuestra cobertura de todas las reglas Friend, DLL,
módulos explícitos o declaraciones sustituidas. El método genérico del ejemplo
`70-method` continúa rechazado y sigue siendo un caso independiente abierto.
Los resultados se obtuvieron con otros cambios locales presentes y no certifican
por sí solos una revisión limpia.
