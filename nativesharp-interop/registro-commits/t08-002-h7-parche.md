# T08-002: corrección propuesta para H7

Se añade `parches/t08-002-h7-excluir-constantes.patch`: una condición en
`appendNamespaceStateMetadata` (`LibraryMetadataConstruction.cpp`) que deja de
publicar las constantes de enum C como estados de espacio de nombres. El parche
**no se aplica** en la rama porque el archivo es compartido.

El parche se midió aplicado temporalmente en el árbol de trabajo de este carril y
después se revirtió:

- los siete productores construyen sin publicar estados;
- los métodos ordinarios y la plantilla con el enum en la superficie ejecutan en
  el consumidor;
- las dos plantillas con un enum fuera de la superficie o anónimo pasan a fallar
  en el consumidor con `unknown identifier`, porque necesitan información
  adicional.

`T08-002-H7-CONSTANTES.md` recoge la causa, el análisis, la medición y el diseño
pendiente para las plantillas. La columna PROP de `constantes-enum/casos.txt`
queda con los valores medidos.
