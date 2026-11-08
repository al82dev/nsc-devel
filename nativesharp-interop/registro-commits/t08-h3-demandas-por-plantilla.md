# T08 H3: conservar demandas por plantilla

La compilación conserva la relación entre cada plantilla y las demandas de
funciones y valores capturadas por el observador. La asociación utiliza la
identidad del nodo sintáctico y el ordinal del proyecto, y conserva el contexto
de búsqueda completo que ya calculó NSC. Se limpia al reconstruir el modelo y
solo se crea cuando hay sesiones C/C++ y un proveedor que utiliza el observador.

Son candidatos: una demanda puede corresponder a un local, un miembro u otro
símbolo y no se publica como dependencia C por el mero nombre. Los tipos no se
resuelven mediante esta lista. El control existente se amplió para verificar
llamada, global y local con sombra bajo sus plantillas originales.

Pendiente: compilar y ejecutar el control agrupado con la conexión del catálogo;
el cambio aún no publica metadata ni acredita consumo precompilado.
