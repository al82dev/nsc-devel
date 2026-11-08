# TLS sin importaciones: contraste con anfitrión convencional

La prueba construye dos DLL C desde la misma fuente y las carga en procesos
Python separados mediante `ctypes`, sin utilizar el ejecutable de control
NativeSharp ni su punto de entrada. Comprueba el hilo principal y otro hilo.

En esta máquina, la DLL sin importaciones devolvió `-2147483648` en ambos
hilos; la DLL con una importación Win32 no invocada devolvió 7 en ambos. La
tabla de importaciones se comprobó con `llvm-readobj`. El resultado descarta
que el comportamiento dependa exclusivamente del ejecutable de control
anterior, pero no determina la causa interna ni corrige la DLL sin importación.
La prueba concluye con código 0 al reproducir esas observaciones.
