# 326 — Promoción y resolución de miembros anónimos / Anonymous-member promotion

## Resultado / Outcome

NativeSharp acepta `struct { campos };` y `union { campos };` dentro de clases, structs y unions.
Los fields de instancia del cuerpo se resuelven directamente desde el contenedor, incluso a través
de varios agregados sin nombre. La forma con campo explícito de los puntos 324–325 conserva acceso
calificado y no promueve.

NativeSharp accepts unnamed inline structs and unions and promotes their instance fields directly
into the containing type. Promotion is recursive; explicitly named carriers remain qualified.

## Modelo semántico y físico / Semantic and physical model

- El parser conserva un tipo anidado sintetizado y un field carrier oculto por agregado.
- `ProjectMemberModel` crea aliases promovidos no físicos con la ruta ordenada de carriers y el
  field fuente final.
- Sólo se promueven fields de instancia. Métodos, estáticos y modificadores incompatibles reciben
  diagnóstico.
- Un nombre directo o dos rutas promovidas con el mismo nombre son un conflicto.
- La accesibilidad efectiva es la intersección conservadora de cada tramo.
- `ProjectObjectLayoutModel` deriva el offset sumando carrier(s) y field fuente después de completar
  todos los layouts físicos.
- Los aliases no participan en listas físicas, inicialización ni destrucción; no duplican storage.

## Fronteras C y precompilada / C and precompiled boundaries

El header C/MSVC emite `struct` y `union` anónimos reales, oculta nombres sintetizados y afirma con
`offsetof` los nombres promovidos. El contrato se compila como C11 y C++20 con Clang para Windows
x64.

NSMETA avanza a 1.17 usando un bit de field para identificar el carrier anónimo. El materializador
de `LibraryReference` reconstruye recursivamente la sintaxis y el layout del productor. Al escribir
1.16 se elimina el bit, ya que ese formato no prometía promoción. No cambian MIR 1.6, Object ABI 2,
Runtime ABI, runtime, ownership, bindings ni tutoriales.

## Evidencia / Evidence

`Item326AnonymousMemberPromotionTests.cpp` verifica 37 condiciones:

- promoción de structs y unions, incluida una ruta recursiva;
- lecturas, escrituras, arrays superpuestos y `offsetof` en O0;
- `pack(1)` sin aplanar el carrier físico;
- ausencia de fields promovidos duplicados en el layout;
- conflictos directos y entre promociones, accesibilidad y categorías no-field;
- separación de la forma nombrada;
- header C sin nombres internos;
- round-trip actual y downlevel de NSMETA;
- productor estático, consumidor `LibraryReference`, enlace y ejecución a través de la ABI.

Las regresiones adyacentes cubren los puntos 322, 324 y 325, el generador de headers C/C++ y la
matriz histórica completa de NSMETA.

## Mensaje de commit / Commit message

`Promueve miembros de agregados anónimos / Promote anonymous aggregate members`
