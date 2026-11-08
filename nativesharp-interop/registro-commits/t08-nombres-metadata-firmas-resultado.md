# Resultado del productor con firmas C

`validar-referenciados.cmd` terminó con código 0 en O0 usando el compilador
incremental `7dc4a8491734454f`. El productor declara `NAlias*` y `NMood` en
firmas públicas. La búsqueda de cadenas en `CNamesReferenced.nsmeta` encontró
`NRecord` y `NMood`, pero no encontró `NAlias`.

Esto distingue el enum referenciado, que sí aparece en el archivo, del alias
referenciado, cuyo nombre no aparece. La inspección no demuestra que el enum
restaurado sea utilizable por un consumidor; tampoco prueba todas las formas
de alias. El caso original sin referencias sigue fallando y la tanda 8 abierta.
