# T08-002: pruebas dirigidas de identidad C

Nuevo ejecutable `nsc_cpp_c_identity_tests` (`tests/unit/CppCIdentityTests.cpp`),
dado de alta en `CMakeLists.txt` junto a `nsc_cpp_session_tests` y con las mismas
dependencias de enlace. No modifica pruebas ni fixtures existentes.

Modo de sondas (`<recursos-clang> <trabajo> <clang>`):

1. Abre sesiones Clang reales sobre `a.h` (dos veces) y `b.h`, que tienen el
   mismo texto, y comprueba para once categorías (función externa y
   `static inline`, global externa y `static`, typedef, struct, struct nombrado
   solo por typedef, union, campo, enum, enumerador y enum anónimo) que el USR
   existe, que es igual entre sesiones independientes del mismo encabezado y si
   depende o no del nombre del encabezado.
2. Instala `a.h` y `b.h` en un mismo proyecto y comprueba que la función y la
   global externas forman un grupo o candidato con USR, y que el struct y el
   enum con etiqueta conservan un grupo y USR. Se imprimen las observaciones de
   la función `static inline` y de la global `static`.
3. Control: el typedef visto por dos sesiones del mismo encabezado enlaza. El
   caso `a.h` + `b.h` solo se observa (hallazgo H1 de la auditoría).
4. Observación de dos enums anónimos distintos con el mismo primer enumerador.

Modo de restauración (`--verify-restored-c-usr <nsmeta> <encabezado> <recursos>
<clang> <trabajo>`): lee y autentica el NSMETA de un productor `nsc` real,
envuelve su catálogo en un módulo precompilado como hace el driver, configura el
selector y los proveedores de campos y valores del consumidor, compila los
miembros de un consumidor sin encabezado C propio y compara con Clang el USR de
cada record, campo, enum y enumerador restaurado. El consumidor demanda todos los
enumeradores del catálogo.

Las expectativas de dependencia del encabezado son las que observa Clang 23.1.0.
La primera ejecución esperaba que el enum anónimo dependiera del archivo; Clang
lo nombra por su primer enumerador (`c:@Ea@X@X`) y la expectativa se corrigió
antes de este commit. Resultados: `t08-002-resultados-identidad-c.md`.
