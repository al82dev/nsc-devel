# T08-002: valor TLS inicial en una DLL sin importaciones: causa operativa y decisión pendiente

Fecha: 2026-11-08. Casilla: «Determinar la causa y resolver el comportamiento sin depender de añadir una
importación artificial a la DLL».

## Lo que demuestran las pruebas existentes

- [t08-dll-sin-import-inspeccion-tls](t08-dll-sin-import-inspeccion-tls.md),
  [t08-dll-tls-importacion-no-invocada](t08-dll-tls-importacion-no-invocada.md),
  [t08-dll-tls-directorio-importacion](t08-dll-tls-directorio-importacion.md) y
  [t08-tls-sin-import-host-estandar](t08-tls-sin-import-host-estandar.md) muestran que:
  - una DLL C con TLS y **sin directorio de importación** recibe el índice TLS 0 y lee `0x80000000`;
  - la misma DLL con un directorio de importación que solo contiene el descriptor nulo recibe un índice
    propio y lee su valor inicial;
  - pasa igual con importación directa, con carga dinámica, con un anfitrión Python (`ctypes`) sin
    NativeSharp, en el hilo principal y en otro hilo.
- En esta tanda, `validar-variadicas-agregados.py` lo volvió a observar: las funciones de una DLL sin
  importaciones devolvían un valor TLS incorrecto, y con una importación de kernel32 el valor era el
  correcto.

## Causa operativa

En este Windows, el cargador inicializa el TLS estático de una DLL (asigna `_tls_index` y copia la
plantilla) solo si el módulo tiene directorio de importación. No hace falta ninguna dependencia, basta
el descriptor nulo. Sin él, `_tls_index` queda en 0 y el código de la propia DLL lee el bloque TLS del
ejecutable. NativeSharp no interviene: el anfitrión `ctypes` lo reproduce. No se determinó la rutina
interna de ntdll, porque eso exige analizar el binario del sistema y queda fuera del alcance acordado.

## Por qué NSC no puede corregirlo por sí solo

El valor incorrecto lo lee el propio código de la DLL, con su `_tls_index`. NSC solo podría corregirlo
de dos maneras:

- reescribiendo la DLL (añadir el descriptor), que altera una biblioteca de terceros, su identidad y su
  firma, algo que ya se descartó en la evidencia anterior;
- o asignando un índice y escribiendo `_tls_index` dentro de la imagen cargada, un parche en memoria
  frágil que no cubre a otros consumidores de la misma DLL.

## Decisión pendiente

Dos diseños con efectos visibles distintos:

1. Diagnóstico al enlazar o empaquetar: si una DLL C con directorio TLS no tiene directorio de
   importación, NSC informa que Windows no inicializará su TLS. Puede ser un error o una advertencia, y
   esa elección es parte de la decisión.
2. Aceptar la limitación del sistema y documentarla, sin cambio en NSC.

Mientras tanto, las pruebas que usan DLL C con TLS importan una función de kernel32, como ya hacían.
