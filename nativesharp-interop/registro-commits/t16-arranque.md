# T16-001 a T16-005, T16-008, T16-012 y T16-013: arranque, constructores y destructores globales

## Comprobación

Este cambio comprueba el arranque con bibliotecas C++; el código de producción es el del
[cambio 1](t16-inicializadores.md) y el `--crt-startup` de la tanda 15.

## Diseño

- **Punto de entrada y entradas del enlace** (T16-001, T16-002): sin `--crt-startup` el ejecutable entra por
  `__nativesharp_start`; con `--crt-startup`, por `mainCRTStartup`, que llama a un `main` que entra en
  NativeSharp, con el runtime estático de MSVC (`libcmt`, `libvcruntime`, `libcpmt`, `libucrt`, `kernel32`)
  dado con `--link-lib` y lld-link.
- **Constructores y destructores** (T16-003, T16-004): los ejecuta el CRT. Con `--crt-startup`, `_initterm`
  recorre `.CRT$XI*` y `.CRT$XC*` antes de `main`, y `exit` llama a los manejadores de `atexit` y a los
  destructores globales con el resultado de `Main`. Sin `--crt-startup`, un enlace que tomaría un miembro con
  inicializadores se rechaza (cambio 1). Una DLL C++ los ejecuta en su propio arranque.
- **Orden** (T16-008): runtime de C y C++, constructores globales C++, runtime NativeSharp (excepciones y
  `Main`, dentro de `main`), y al salir manejadores de salida y destructores C++. Un constructor global C++
  corre antes de que exista el runtime NativeSharp, así que no debe llamar a código NativeSharp; las DLL
  importadas se inicializan antes de la entrada del ejecutable.
- **DLL y `DllMain`** (T16-012, T16-013): la DLL C++ enlaza su propio runtime estático; el cargador llama a
  `_DllMainCRTStartup`, que construye sus globales al cargarse y los destruye al descargarse, bajo el lock del
  cargador. Ahí rigen las restricciones de `DllMain` (no cargar otras DLL, no esperar hilos ni llamar a
  NativeSharp); NSC no ejecuta código propio en `DllMain`.

## Evidencia

Fixture nuevo `pruebas-tanda-16-arranque/validar.py`, con y sin `--dll`: un objeto global con constructor y
destructor y un entero de inicialización dinámica; el constructor, `Main` y el destructor escriben una
letra en `marca.txt` con Win32 (`CreateFileA`/`WriteFile`), una observación independiente del CRT.
Resultado 42 con marca `cmd`, igual que el control C++ de Clang 23.1.0, O0–O3.

Controles negativos (estática, O0):

- sin `--crt-startup` el ejecutable se rechaza nombrando el miembro `api.obj`;
- una DLL NativeSharp (`--emit-project-oop-dll`) con esa biblioteca se rechaza con su motivo;
- contraparte: con una biblioteca de dos miembros, si el programa usa solo el que no tiene inicializadores
  enlaza sin `--crt-startup` y devuelve 42; si usa el otro, se rechaza nombrando `started.obj`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática (`--crt-startup`) | pasa (O0–O3) y negativos | `A:/temp/nsc-t16-arranque-fpeo7o7x` |
| DLL (entrada NativeSharp) | pasa (O0–O3) | `A:/temp/nsc-t16-arranque-ccahldl2` |

## Límites

- La DLL se carga al iniciar el proceso y se descarga al salir; `LoadLibrary`/`FreeLibrary` desde
  NativeSharp no se probó.
- Llamar a NativeSharp desde un constructor global C++ no está soportado (corre antes del runtime
  NativeSharp) y no tiene diagnóstico.
- El runtime dinámico de MSVC (`/MD`) no se probó.
