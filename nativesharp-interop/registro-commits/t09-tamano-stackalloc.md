# T09: tamaño oculto de reservas de pila directas

NSC rechazaba pasar directamente `stackalloc` a un parámetro C o C++ que
solicita el tamaño de subobjeto en los modos 1 y 3. Ahora conserva el tamaño
de una reserva constante y deja que LLVM calcule el tamaño dinámico cuando el
modo lo permite. Para el modo 3 utiliza el resultado desconocido observado
con Clang, incluso en la reserva constante probada. La reserva y sus
dimensiones se evalúan una sola vez por el flujo normal del compilador.

## Validación

- Construcción incremental de `nsc` y comprobación del runtime del host.
- Las pruebas `validar-stackalloc.py` de C y C++ contrastan con Clang
  reservas directas de tres enteros y reservas de tamaño variable en los
  cuatro modos estáticos y dinámicos, O0–O3, convenciones predeterminada y
  SysV, bibliotecas estáticas y DLL. Cada combinación de lenguaje y
  convención incluye dieciséis consumidores precompilados, con y sin header.
- C: `A:/temp/nsc-t09-c-size-stackalloc-lf0aaw16` y
  `A:/temp/nsc-t09-c-size-stackalloc-_ej4fujd`.
- C++: `A:/temp/nsc-t09-size-stackalloc-9mwnf8h1` y
  `A:/temp/nsc-t09-size-stackalloc-3macaplt`.
- Una reproducción adicional compara la dimensión calculada mediante una
  llamada directa en modo dinámico 1: `A:/temp/t09-stack-direct-call-mode1-probe`.

## Límites

Las pruebas cubren las reservas y modos descritos en Windows x64. No
acreditan todas las dimensiones, tipos, expresiones de tamaño o convenciones,
ni otras reservas dinámicas. T09-003 permanece abierto.
