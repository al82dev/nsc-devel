# T08-010/015: modo consumidor en el grafo de proyectos

El cargador reconoce `CppConsumerVolatileMode` en `PropertyGroup`, acepta solo
`Microsoft` o `ISO`, rechaza valores inválidos o duplicados y conserva la
selección en cada `Project`, independientemente de los encabezados. Ausencia
permanece sin especificar.

La generación HIR rechaza temporalmente cualquier proyecto que indique la
propiedad, porque todavía no consume ese valor. Así no se acepta una opción y
se genera código como si hubiera sido aplicada. El control es provisional y se
retirará cuando la selección llegue a los accesos y a sus identidades de caché.
No se ha construido ni validado este cambio.
