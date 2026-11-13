# SysV: enum C++ global en record precompilado

El productor de la muestra inicialmente no podía guardar el enum C++ de un
campo: el catálogo lo excluía y asumía que todo enum restaurado era C. Tras
corregir la captura, el consumidor necesitó además reconstruir el enumerador
con su tipo C++ y conciliar el campo con el plan ABI del catálogo.

El contrato vigente conserva ahora explícitamente el origen C o C++ del enum;
la representación de los enums C existentes permanece igual. Se verifica
identidad USR, módulo, destino, base y disposición antes de admitir el campo
en el plan SysV. La prueba de identidad del catálogo verifica recuperación del
origen y rechaza un enumerador con USR de tipo distinto. No se añadió una
selección de contratos según etiquetas históricas.

`pruebas-tanda-09-sysv-cpp-copia-enum/validar-precompilado.py` pasó las 16
combinaciones de estática y DLL, O0–O3, con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-9ztftk1x`. La prueba
viva volvió a pasar sus ocho combinaciones:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-pq_83umr`. También pasaron los
controles dirigidos de identidad, ABI y transporte, y la compilación
incremental terminó con código 0.

La evidencia corresponde a un enum global no scoped con base `int`, en un
record trivial concreto. No cubre enums de namespace, otras bases ni todas las
formas de metadata o ABI. T09-003 sigue abierto.
