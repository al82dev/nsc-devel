# Procedencias múltiples en punteros reenviados

La corrección `23a750a6e` recuperaba un origen de un alias, pero la unión de
orígenes podía conservar solamente uno cuando el alias se reasignaba entre dos
parámetros. En la reproducción, el llamador pasaba una dirección local por el
segundo parámetro y el análisis aceptaba la llamada a una biblioteca con
contrato desconocido (`A:/nsc-fixes-evidencia/unknown-pointer-multiple-before.log`).
Una reasignación mediante aritmética también perdió la procedencia
(`unknown-pointer-arithmetic-before.log`). Los programas solo se analizaron.

Para locales de puntero, el análisis de asignaciones conserva enlaces a locales
y parámetros posibles. Al construir dependencias de escape para un parámetro
puntero por valor, el compilador recorre esos enlaces y los inicializadores,
con conjuntos de visitados, y añade dependencias para los parámetros puntero
identificados por enlaces semánticos. Los demás tipos conservan su camino
anterior. El análisis es conservador y puede conservar fuentes de asignaciones
anteriores aunque se hayan sobrescrito; no afirma precisión temporal exacta.

La compilación incremental y tres controles dirigidos pasaron
(`unknown-pointer-multiple-final-controls.log`, 3/3). La prueba de resúmenes
pasó 29 comprobaciones: segundo parámetro, cadenas, ramas, condicional,
aritmética, un parámetro ignorado con puntero global y el control `NoEscape`.
También pasaron los controles seleccionados de Rc/Arc y Core binario
(`unknown-pointer-multiple-final-pointers.log` y
`unknown-pointer-multiple-final-binary.log`). Los registros están en
`A:/nsc-fixes-evidencia`. El compilador incorpora otros cambios provisionales;
la evidencia no certifica una revisión limpia ni todas las formas de alias.
No se trabajó en interoperabilidad C/C++ ni se ejecutó la batería completa.
