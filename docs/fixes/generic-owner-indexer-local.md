# Identificadores locales en indexadores sobrecargados

Con `const int index = 1` y getters `this[byte]` y `this[long]`, el control
ordinario selecciona `long`; la lectura del local no transporta una constante
de conversión en el análisis semántico. La llamada directa al método genérico
sobre `items[index]` se rechazaba antes de cerrar el dueño. En O0 y O2 el
control devuelve 42 y la llamada directa falla; comando y diagnósticos en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-conversions-indexer-constant-fix/results.json`.

La selección numérica descarta una sobrecarga incompatible para un
`IdentifierExpression` solo cuando el contexto genérico conoce su binding.
Se usa la clasificación existente sin constante, como hace la lectura
semántica de locales, parámetros y campos sin calificar; no se calcula ni se
inventa el valor del identificador. Para expresiones sin información suficiente
se conserva la ruta conservadora anterior.

Con el compilador experimental, el control y la llamada directa devuelven 42
en O0 y O2 (`generic-indexer-conversions-indexer-local-fix/results.json`). La
regresión `generic_owner_indexer.py` cubre valores locales constantes positivo
y negativo y un parámetro, en fuente y biblioteca estática/DLL O0/O2. También
pasaron los controles dirigidos previos de indexadores, sobrecargas, métodos,
matriz, clausura binaria, restricciones e imports; los logs están en
`A:/nsc-fixes-agente-2-evidencia/generic-indexer-local-fix/`.

El ejecutable de prueba combina tres objetos recompilados con bibliotecas
copiadas del build de campaña; el comando y los hashes están en su `build.log`.
No es un build limpio. Falta validar con el compilador integrado. La expresión
constante no literal y el diagnóstico de ambigüedad no se consideran resueltos
por esta corrección.

Un cambio posterior diagnostica ciertas ambigüedades demostrables; véase
`generic-owner-indexer-ambiguity.md`. Las demás condiciones siguen pendientes.
Verificación integrada, 2026-11-11: la compilación incremental de `nsc`
terminó correctamente y `ctest --test-dir A:\nsc-fixes-evidencia\build -R
nsc.generics.owner-method-indexer -V` pasó 1/1 sin omitir la prueba. El log
`A:\nsc-fixes-evidencia\generic-indexer-conversions-20261111.log` incluye
las variantes de este cambio y del cambio de conversiones anterior. El
compilador contenía otros cambios locales: no es una validación de un árbol
limpio ni demuestra compatibilidad general de todos los indexadores.
