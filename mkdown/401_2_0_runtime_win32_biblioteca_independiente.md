# Item 401 — Runtime Win32 como biblioteca independiente

## Resultado

- El runtime reutilizable se genera como el archivo COFF determinista
  `NativeSharp.Runtime.Win32.lib`, con formato `1`, y se instala en
  `lib/nativesharp/runtime/`.
- La biblioteca contiene el soporte completo del Runtime ABI `1.0`, pero no captura
  `__nativesharp_start`: cada ejecutable recibe un objeto de entrada pequeño ligado a su propio
  `Main`.
- El driver oficial configura siempre la biblioteca instalada. La variable
  `NSC_WINDOWS_RUNTIME_LIBRARY` permite sustituirla únicamente por otro archivo que satisfaga la
  verificación del contrato.
- Las bibliotecas estáticas de usuario ya no llevan una copia privada del runtime. Los ejecutables
  y las DLL resuelven el soporte contra el archivo independiente.
- Una DLL sin código materializado conserva un objeto ancla mínimo y privado; así sigue siendo un
  PE válido aunque el enlazador no necesite extraer ningún miembro del archivo de runtime.
- `nsc --write-windows-runtime-library` reproduce el archivo y
  `nsc --verify-windows-runtime-library` comprueba su superficie ABI antes de publicarlo o usarlo.

## Validación

- Prueba dedicada `nsc.item401.independent-windows-runtime`: creación y verificación del archivo,
  separación del objeto de entrada, enlace y ejecución de un PE nativo, y ausencia de símbolos del
  runtime dentro de una biblioteca estática de usuario.
- Regresiones del runtime y del enlazador Windows, driver, enlace incremental, bibliotecas nativas,
  matriz cruzada de DLL, hosts C/C++ estáticos con `lld-link` y NSLink, y manifiesto de instalación.
- Construcción limpia completa, instalación en `C:\native-sharp` y verificación directa del
  artefacto recién instalado.
