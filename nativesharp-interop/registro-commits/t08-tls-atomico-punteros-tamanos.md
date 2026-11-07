# T08: punteros TLS atómicos a elementos de dos y ocho bytes

NSC rechazaba los globales C `_Thread_local _Atomic(short*)`, `_Atomic(long long*)` y `_Atomic(double*)`. Se proyectan ahora únicamente con los tamaños comprobados por Clang y almacenamiento de puntero de 64 bits alineado; HIR y LLVM conservan lecturas y escrituras atómicas y usan actualizaciones `seq_cst` con desplazamientos de dos u ocho bytes. Los apuntados const y volatile de estos tres tipos aún se rechazan.

El control C compilado por Clang comprobó los resultados sobre arreglos reales. NativeSharp pasó lectura, suma, incremento y resta O0–O3 para los tres tipos; el verificador de IR encontró los accesos y desplazamientos atómicos esperados. Se comprobó que el apuntado `volatile short` aún se rechaza. Pasó el fixture agrupado TLS previo antes de añadir este último control negativo, que se ejecutó por separado.

No se modificaron NSLink, Clang ni C:. Los cuatro fallos DLL y la validación integral permanecen abiertos; esto no cierra la tanda.
