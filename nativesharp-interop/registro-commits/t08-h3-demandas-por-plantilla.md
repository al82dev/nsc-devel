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

Validación: se construyó incrementalmente `nsc_cpp_c_identity_tests` y se
ejecutó con sesiones Clang 23.1 y temporales en A:; ambas operaciones terminaron
con código 0. El control confirmó la asociación de la llamada, la global y el
local sombreado a sus plantillas. La construcción emitió tres advertencias de
inicializadores incompletos en `CppSemanticRecord.cpp`, fuera de este cambio.

La conexión del catálogo sigue pendiente. El cambio no publica metadata ni
acredita consumo precompilado; no se ejecutó la batería completa de la tanda 8.
