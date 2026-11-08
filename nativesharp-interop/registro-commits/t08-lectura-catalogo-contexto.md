# Lectura del catálogo C y contexto del módulo

Dos controles del catálogo C no podían leer enums publicados porque el lector
sin contexto asumía un valor fijo que no correspondía al catálogo producido.
Cambiar solo ese valor hizo visibles fallos adicionales: un módulo creado en
memoria también tenía un valor fijo antiguo para su catálogo.

El lector sin contexto valida el contenido completo y rechaza dos
interpretaciones válidas en lugar de elegir una arbitrariamente. Cuando existe
contexto explícito se conserva la ruta correspondiente. Los módulos nuevos en
memoria usan la constante del contrato vigente del compilador. No se creó otro
formato ni se modificaron las pruebas existentes de NSMETA.

La construcción incremental terminó y `nsc_cpp_session_tests` pasó con código 0.
El consumo de enums C de dos DLL y el rechazo de valores incompatibles pasaron
en O0. La validación completa de la tanda 8 todavía no se ha ejecutado; los
demás pendientes del tracking siguen abiertos.
