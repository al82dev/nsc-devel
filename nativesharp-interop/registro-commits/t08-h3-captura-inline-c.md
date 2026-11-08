# T08 H3: captura de funciones C static inline

La captura de dependencias rechazaba dos definiciones `static inline` de C
obtenidas de encabezados distintos. La reproducción empleó las sesiones Clang
reales de `CppCIdentityTests`: ambos grupos estaban disponibles, pero su captura
fallaba con `selected C function has no matching observation`.

Clang devuelve `NoLanguageLinkage` para las declaraciones sin enlace externo
formal (`clang/lib/AST/Decl.cpp`, `getDeclLanguageLinkage`); la sesión de NSC
marca separadamente una definición estática inline procedente de C. El filtro
de `captureCDependencyFunction` exigía enlace de lenguaje C y la descartaba.
La captura y el codec interno admiten ese caso concreto y restauran su
`NoLanguageLinkage`, conservando USR, referencia y símbolo interno seleccionado.
Las funciones ordinarias continúan exigiendo enlace de lenguaje C.

Se reprodujo el fallo antes del ajuste. Tras la corrección se construyó
incrementalmente `nsc_cpp_c_identity_tests` y su ejecución terminó con código 0;
el control contrasta los dos encabezados y la ida y vuelta de cada definición.
La compilación mostró dos avisos de inicializadores incompletos en líneas
ajenas al cambio. Este codec todavía no se publica en NSMETA: no se acredita
consumo precompilado ni cierre de la tanda 8.
