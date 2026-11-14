# T09-003: símbolo explícito de fábrica C++

Se comprobó una fábrica C++ `makeAsm(int) -> Probe*` declarada con `__asm__("T09_explicit_factory")`. Su resultado se utiliza para llamar un método de instancia con tamaño de objeto oculto. La prueba ejecuta la llamada directa y una llamada encadenada desde plantilla precompilada, con y sin encabezado, mediante biblioteca estática y DLL, en O0–O3; tanto Clang 23.1.0 como NativeSharp devolvieron el resultado esperado.

Los ocho objetos compilados por Clang definen exactamente `T09_explicit_factory`, según `llvm-nm`. En el IR volcado por NativeSharp, la llamada y la declaración emplean ese símbolo; para la DLL la declaración incluye `dllimport`. La comparación exacta quedó en los archivos `O*-simbolo-comprobado.log` dentro de `A:/temp/nsc-t09-instance-object-size-fhbtyru7` (estática) y `A:/temp/nsc-t09-instance-object-size-xnjygrwx` (DLL), junto con los logs de ejecución. El IR volcado se comprobó por cada proyecto de prueba; no se afirma que sea IR optimizado para cada nivel.

El fixture conserva comprobaciones de la declaración y llamada LLVM y del símbolo en el objeto. No se modificó producción. La muestra no certifica todos los nombres explícitos, convenciones ni formas de función; T09-003 y la tanda 9 siguen abiertos.
