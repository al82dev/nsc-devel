# T08 H3: ejecución del filtro de catch

El control de filtro anterior solo compilaba la cláusula sin lanzar. Esta
variante lanza una excepción y espera entrar en el filtro y el handler desde el
consumidor precompilado; el validador sigue comprobando que la global C
homónima no se publique. Se incluye un catch general para cubrir el caso en que
el filtro sea falso, como exige el análisis de excepciones.

Con NSC build `1965527e723a8dbb`, `CALL diagnosticar.cmd` terminó con código 0
en O0–O3: el consumidor obtuvo 32 al lanzar, y pasaron las demás comprobaciones
agrupadas. La invocación directa del script informó código 0 ante un error;
por ello la evidencia usa `CALL`. La tanda 8 sigue abierta.
