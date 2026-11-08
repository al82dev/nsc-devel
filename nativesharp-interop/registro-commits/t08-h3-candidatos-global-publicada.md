# T08 H3: candidatos de global para plantillas publicadas

La selección conserva para cada demanda de una plantilla publicada el proyecto,
namespace y resultado del selector de globales, incluidos los índices de
declaraciones, alias, orígenes internos y conflictos. Los resultados siguen
siendo candidatos: la sombra de un local se decide durante el análisis de cuerpo.

El control con dos sesiones Clang encontró las dos declaraciones de la global
referida por la plantilla publicada y excluyó la otra plantilla. La construcción
incremental y `nsc_cpp_c_identity_tests` terminaron con código 0. Se emitieron
siete advertencias de inicializadores incompletos en código previo fuera de la
modificación. Los candidatos aún no se publican en NSMETA ni se han probado en
un consumidor precompilado; la tanda 8 sigue abierta.
