Este fixture conserva `Types.ns`, la biblioteca y la metadata generadas por el
compilador base de la campaña, build hash `14717501b74375ce`. Los operadores
publican un campo owned de su operando; se usan únicamente para comprobar que
el consumidor rechace temporales, sin ejecutar el código inseguro.

SHA-256 `Producer.lib`: `e0c89507924728b9524647b518c62e11733951b960336ce97e19dd887b8ed836`.

SHA-256 `Producer.nsmeta`: `7122789f4d72bad33a225549c9ee166f3178a821d59533a3a3ead1a92c7619c0`.

El comando original y su salida están en
`A:/nsc-fixes-evidencia/70-binary-escape-variants/build-producer.cmd` y
`producer.log`. No regenerar el fixture con el compilador actual: este rechaza
las tres implementaciones inseguras. La prueba también analiza controles con
locales, pero no los ejecuta ni declara seguros sus efectos posteriores.
