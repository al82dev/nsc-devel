# T08-010 / T08-012 / T08-016: arreglos escalares C empaquetados

El proyector admite arreglos fijos C subalineados cuando todos los niveles
están completos y no calificados y el elemento es un escalar representable.
El análisis de miembro impide exponer la dirección del campo si su elemento
requiere mayor alineación; el análisis del índice protege sus elementos y LLVM
ya conserva la alineación a través de los strides. Se mantienen las barreras
para arreglos calificados, de punteros, records, enums y C++ subalineados.
La ejecución mixta y la batería de la tanda siguen pendientes.
