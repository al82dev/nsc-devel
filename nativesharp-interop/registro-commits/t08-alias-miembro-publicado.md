# Alias C en miembros publicados

El productor `CNamesReferenced` usa `NAlias*` en un miembro público pero su
catálogo no conserva `NAlias`. El proveedor de metadata ahora agrega los
alias efectivamente resueltos de los tipos de miembros que aparecen en la
metadata, incluidos parámetros y tipo de error, al catálogo C existente.
Se seleccionan por identidad del miembro, nombre, firma y clase; se omiten
miembros precompilados y externos. No se cambia el formato del catálogo ni se
publican alias de miembros ajenos a la metadata.

La construcción incremental y los controles de consumo quedan pendientes.
Este cambio no cubre todavía funciones de namespace ni alias no usados por
la API y no cierra la tanda 8.
