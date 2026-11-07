# T08-002: metadata para plantillas públicas que usan declaraciones C

El fixture `pruebas-tanda-08-identidad-c/plantillas` pasa a tener trece casos
con biblioteca precompilada (`casos.txt`): funciones, globales leídas y
escritas, typedef, record local o de la superficie, `static inline` y
constantes de enum con nombre y anónimo, en cuerpos genéricos y no genéricos.
Los productores separados aíslan cada fallo.

`T08-002-PROPUESTA-METADATA-C.md` se reescribe con:

- La información exacta: los snapshots de declaración Clang que la proyección
  vuelve a ejecutar en el consumidor, campo por campo.
- La comparación de cuatro opciones de versionado.
- La recomendación (opción B: disposición del catálogo elegida por el minor 46,
  máscara de características y subida condicional) y sus consecuencias.
- La corrección de la versión anterior: la envoltura 4 ya existe.

Dos hallazgos nuevos:

- **H7**: una constante de enum C en cualquier cuerpo de biblioteca impide
  construirla, porque la construcción de metadata la publica como estado público
  sin espacio de nombres.
- **H8**: la sonda 7 muestra que un catálogo 1.42 solo de enums se confunde con
  la envoltura 4.

No se modifica NSMETA ni el código del compilador.
