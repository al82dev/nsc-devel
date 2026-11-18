# Ruta UTF-8 de ASan: bloqueo del build de laptop

Base: `23508249e992c29f4bfa0571ed6e2ceaee73b589`, rama
`development-fixes-limitaciones-laptop`. El usuario autorizó una compilación
inicial aislada; después se usarán únicamente builds incrementales y pruebas
dirigidas. No se modifica el repositorio NSLink ni la instalación funcional.

La configuración Release usa Ninja, Clang/LLVM 23.1.0, enlace estático,
optimización del host O3 y `NSC_WARNINGS_AS_ERRORS=ON`. El build está en
`A:/temp/nsc-fixes-laptop-build`. El comando inicial fue:

```text
cmake --build A:/temp/nsc-fixes-laptop-build --target nsc --parallel 4
```

Ninja detuvo esa compilación en el paso 533/629: el compilador emitió
`STL4021` para `std::filesystem::u8path(reported)` en
`src/Linker/LldLinker.cpp`, y la política de warnings convirtió la advertencia
de deprecación de C++20 en error. Los encabezados observados pertenecían a
MSVC 14.51.36231. La salida envolvente de `start /wait` devolvió cero pese al
fallo de Ninja; se conserva por tanto el diagnóstico de Ninja como resultado.

El cambio se limita a construir `std::filesystem::path` desde un `std::u8string`
formado con los mismos bytes. Su finalidad es preservar la interpretación UTF-8
de la ruta informada por Clang sin usar la función deprecada. No cambia las
opciones de compilación, el contrato público ni el proceso de selección de
runtime. La equivalencia funcional con rutas no ASCII todavía requiere prueba.
Como evidencia estática de la implementación utilizada, el encabezado
`filesystem` de MSVC 14.51.36231 convierte `u8path` mediante
`_Utf8_conversion` y el constructor desde `char8_t` mediante el código de
página UTF-8. Esta lectura no sustituye una ejecución con rutas no ASCII.

La continuación incremental del mismo objetivo compiló `LldLinker.cpp` sin
ese error y enlazó `A:/temp/nsc-fixes-laptop-build/bin/nsc.exe`. La salida
indicó `nsc.exe static host-runtime contract: passed` y generó el runtime en
`A:/temp/nsc-fixes-laptop-build/generated/NativeSharp.Runtime.Win32.lib`.
Esto acredita la compilación y el enlace del host, no la equivalencia de las
rutas UTF-8 no ASCII.

## Incumplimiento detectado en la poscompilación

Después del build se revisó `A:/temp/nsc-fixes-laptop-build/build.ninja`: la
regla `POST_BUILD` invocó el `nsc.exe` recién enlazado por ruta absoluta para
generar contratos y el runtime. Esto contradice el requisito de invocar las
herramientas por nombre mediante el PATH y debió detectarse antes de ejecutar
el build. Los artefactos indicados por esa regla se generaron en el directorio
aislado de `A:`; no se ejecutó deploy ni se modificó la instalación funcional.

La sesión todavía no resuelve `nsc.exe` por PATH. Después de informar el hecho,
el usuario autorizó expresamente una excepción limitada al `nsc.exe` recién
construido para garantizar que se utilice la versión que se está modificando,
y no el compilador estable. Esa excepción permite la invocación del ejecutable
aislado por CMake y en las pruebas; no modifica el PATH ni autoriza elegir otra
instalación de herramientas.

El usuario autorizó después usar únicamente el Clang 23.1.0 ya resuelto por
el PATH (`C:/clang-llvm/bin/clang.exe`) por su ruta en la prueba dirigida de
este arreglo, sin modificar el PATH ni el toolchain. El runtime de ese Clang
se lee desde su instalación; las copias y salidas de la prueba permanecen en
`A:`. La autorización no implica instalar, modificar ni reconstruir Clang.

## Regresión dirigida

`tests/integration/asan_runtime_path_validation.py` invocó el `nsc.exe`
aislado y el Clang autorizado, con un runtime coincidente y una copia alterada
en temporales de `A:`. En O0 y O2 el runtime coincidente permitió construir y
ejecutar un programa con salida 42; la copia alterada se rechazó con
`AddressSanitizer runtime does not match` y salida 2. La prueba terminó con
código 0. Su salida se conserva en `A:/temp/nsc-fixes-asan-path-20261109-2.log`.
No modifica `PATH`, no instala nada y no está registrada en el CMake compartido.

La ruta devuelta por el Clang autorizado era ASCII, por lo que esta ejecución
no demuestra la conversión de una ruta devuelta por Clang con caracteres no
ASCII. La comprobación de la implementación de la biblioteca estándar descrita
arriba es evidencia estática, no un sustituto de esa ejecución.

### Hallazgo separado: ruta de entrada no ASCII

En la primera tentativa, una copia válida del runtime ubicada en un directorio
con `é` fue rechazada con el diagnóstico general de rutas ASan, antes de llegar
a la función modificada. Registro: `A:/temp/nsc-fixes-asan-path-20261109-1.log`.
No se ha comprobado si ocurre en la base ni se ha identificado su causa. No se
modificó el manejo de argumentos o rutas de entrada; ese hallazgo permanece
fuera de esta corrección y no se presenta como resuelto.
