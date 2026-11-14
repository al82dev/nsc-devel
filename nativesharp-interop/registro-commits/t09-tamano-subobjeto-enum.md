# T09: tamaño oculto de enums C y C++ importados

NSC rechazaba el tamaño de un enum importado local o contenido en un campo al
pasarlo a un parámetro con `pass_object_size(1)` o `(3)`. Ahora emplea el tipo
entero verificado de su representación. Durante la prueba se detectó además
que el recorrido de dependencias del catálogo omitía los enums C++ encontrados
en tipos de firmas; se incorpora el enum al catálogo vigente para que la
plantilla precompilada pueda recuperarlo sin encabezado.

## Validación

- Construcciones incrementales de `nsc`; la construcción comprobó el contrato
  del runtime del compilador. El archivo de interop emitió cinco avisos de
  inicializadores preexistentes, sin error.
- Comparación con Clang de enum C de base `unsigned short` y enum C++ de base
  `short`: variable local y campo, los cuatro modos estáticos y dinámicos,
  convención predeterminada y SysV, estática y DLL O0–O3. Las pruebas incluyen
  dieciséis consumidores precompilados por lenguaje y convención, con y sin
  encabezado, además de las regresiones de records y punteros del fixture.
- C: `A:/temp/nsc-t09-c-size-record-p8kb9ooq` y
  `A:/temp/nsc-t09-c-size-record-9yamo5ua`.
- C++: `A:/temp/nsc-t09-size-record-kiytfvyu` y
  `A:/temp/nsc-t09-size-record-om1vt54f`.
- La falla observada antes de corregir la captura precompilada quedó en
  `A:/temp/nsc-t09-size-record-flmviyai`.

## Límites

Se acreditan estas dos bases y estas expresiones en Windows x64. El catálogo
debe seguir verificando la identidad y representación del enum; no se añade
un contrato histórico. Otras bases, expresiones, métodos y convenciones no
quedan certificadas por esta prueba. T09-003 permanece abierto.
