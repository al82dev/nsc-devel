# T08: clasificador contextual de miembro flexible C

Se prepara una categoría de conversión para un miembro flexible importado a
puntero de su elemento. El clasificador requiere la marca de origen y una
ubicación direccionable; los demás casos conservan la clasificación anterior.

Todavía no está conectado a inicializadores, asignaciones, llamadas, retornos
ni HIR. La restricción direccionable no acredita los casos const y volatile;
se requieren rutas que preserven sus calificadores antes de habilitarlos.
