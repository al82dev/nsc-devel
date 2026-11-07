# T08: apuntados const y volatile de punteros TLS atómicos

La proyección de punteros TLS atómicos a `short`, `long long` y `double` rechazaba apuntados `const` y `volatile`. Se conservan ahora esos calificadores; la desreferenciación y el índice de apuntados volatile propagan el contrato de Clang en Windows, con alineaciones de 2 y 8 bytes. El descenso LLVM emite lecturas acquire y escrituras release, como en el control C compilado por Clang 23.1.0.

El fixture C y NativeSharp comparó accesos directos e indexados a los tres tipos. NativeSharp pasó O0–O3, y el IR verificó dos lecturas y escrituras por tipo con su orden y alineación. Se comprobaron por separado tres diagnósticos de escritura a const y tres de pérdida de volatile; el gate incorpora esa cuenta. El fixture agrupado pasó antes de agregar la comprobación mecánica de conteo, ejecutada sobre sus logs.

Se mantiene el rechazo de `_Atomic(long double*)` aún no implementado. Los cuatro controles TLS de DLL y la validación integral siguen abiertos; no se modificó NSLink ni Clang.
