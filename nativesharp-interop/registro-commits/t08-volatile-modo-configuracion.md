# Tanda 8: opción de modo volatile en una referencia C/C++

`CppLibraryReference` acepta `VolatileMode="Default"`, `"Microsoft"` o `"ISO"`. La sesión transmite la opción a Clang, rechaza valores inválidos y distingue el modo en la identidad de configuración. La comprobación que reutiliza sesiones compara también la opción; el valor por defecto conserva el comportamiento anterior.

Esta opción configura la sesión que procesa el encabezado y su código inline. No define todavía una política global para operaciones realizadas en NativeSharp sobre punteros almacenados en locales o recibidos como parámetros. La compilación y la validación agrupada de ambos modos están pendientes en el momento de este commit.
