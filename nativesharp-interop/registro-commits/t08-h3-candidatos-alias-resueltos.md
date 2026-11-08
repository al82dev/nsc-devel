# Tanda 8: distinguir alias resueltos de candidatos

La selección de posibles dependencias de plantillas públicas conserva todos los
orígenes candidatos y agrega la observación de resolución correspondiente a la
misma sintaxis: si existe un binding y qué alias expandió, si lo hay. Una
plantilla no resuelta conserva sus candidatos; la coincidencia de nombres no
se presenta como prueba de que se haya seleccionado una declaración.

La prueba de identidad contrasta estas observaciones con los bindings del
compilador. La publicación en NSMETA y el consumo completo siguen pendientes;
este cambio no las acredita.
