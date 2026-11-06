# Prueba de conflictos por USR

El inventario de sesiones reales se duplica para comprobar que una misma
identidad Clang con otro símbolo externo produce conflicto incluso si cambia
el nombre consultado, tanto en búsqueda directa como indexada. Un control
conserva alias compatibles de un mismo símbolo con USR distintos. La prueba
espera la batería agrupada.
