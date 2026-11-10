# Origen de un puntero reenviado mediante un local

La protección de `ed9a7f90d` no alcanzaba un método fuente que copiaba su
parámetro puntero a un local antes de enviarlo a una biblioteca con contrato
desconocido. El llamador podía entregarle una dirección del marco y el análisis
la aceptaba (`A:/nsc-fixes-evidencia/unknown-native-forwarded-alias-before.log`).
La reproducción se analizó y no se ejecutó.

Al construir el grafo de escape, el compilador clasificaba el slot del alias
en vez del valor que contiene, por lo que omitía la dependencia con el parámetro
original. Para parámetros puntero pasados por valor, el grafo utiliza ahora el
evaluador de origen del valor que ya se usa para validar llamadas. Las demás
clases de parámetro conservan su ruta anterior.

Después de una compilación incremental, la selección de tres pruebas pasó
(`unknown-native-forwarded-alias-controls.log`, 3/3). La prueba de resúmenes
pasó 23 comprobaciones, incluidos el reenvío por alias, un alias que apunta a
almacenamiento global y un método que realmente no retiene el argumento.
También pasaron los controles dirigidos de Rc/Arc y Core binario
(`unknown-native-forwarded-alias-pointers.log` y
`unknown-native-forwarded-alias-binary.log`). Los registros están en
`A:/nsc-fixes-evidencia`; el compilador contiene otros cambios locales, por lo
que la evidencia no certifica una revisión limpia ni todas las formas de alias.
No se ejecutó la batería completa ni se trabajó en interoperabilidad C/C++.
