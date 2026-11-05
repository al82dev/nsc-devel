# Tanda 08: aliases disponibles al análisis de nombres

`ProjectCompilation` entrega a `ProjectNameLookup` las observaciones cargadas;
el modelo las conserva con sus datos de procedencia y no acepta una sustitución
después de construir módulos. La resolución y la expansión siguen pendientes;
almacenar las observaciones no hace que un alias pueda utilizarse. Este cambio
se verificará con la validación de la tanda completa.
