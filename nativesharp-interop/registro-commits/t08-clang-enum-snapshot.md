# Tanda 08: captura de enums desde Clang

La sesión copia enums y enumeradores de la unidad, namespaces y linkage,
incluidos enums anónimos y declaraciones incompletas. Los valores usan la
representación decimal y signo de APSInt; el tipo subyacente solo se registra
si Clang lo conoce. Esta captura no los hace utilizables en NativeSharp ni cubre
enums anidados en records. Pendiente de validación agrupada.
