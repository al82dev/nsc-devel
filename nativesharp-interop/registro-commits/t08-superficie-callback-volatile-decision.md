# T08-002: firma pública con callback `volatile`, materializada desde metadata (decisión pendiente)

Fecha: 2026-11-08. Casilla: «Representar en la superficie pública materializada desde metadata una firma
de callback con `volatile` (hoy se rechaza explícitamente)».

## Evidencia

La sonda `claude/sondas/surface1` reutiliza `validar-callback-const.py` y cambia `TakeFn` por
`int (*)(volatile Payload*)`. El productor precompilado publica `public static TakeFn Callback()`. El
consumidor sin encabezado informa:

```
error: receiver/type 'type:ProducerSurface::ConstSurface.Bridge' has no compatible member named 'Callback'
```

`MetadataSourceMapping::sourceCoreTypeForMetadataKey` no puede escribir la firma en código NativeSharp,
así que el miembro no se materializa. Con `const`, la misma superficie funciona desde
[t08-callback-const-identidad](t08-callback-const-identidad.md), porque NativeSharp sí tiene `T const*`.

## Por qué es una decisión

NativeSharp no tiene sintaxis para un dato apuntado `volatile` en un tipo escrito: el parser solo
reconoce `const` postfijo. Hay dos diseños con efectos distintos para el usuario:

1. Añadir al lenguaje una sintaxis `T volatile*`, y que la materialización la escriba como escribe
   `T const*`. Es un cambio del contrato público del lenguaje.
2. Materializar estos miembros por un camino que no pase por código fuente, de modo que conserven la
   identidad `fn:` con su sufijo `:volatile-pointee:`. Cambia cómo se construye la superficie
   precompilada.

Mientras tanto, el miembro no se publica; no se pierde la calificación en silencio. Una mejora menor e
independiente sería un diagnóstico que nombre la causa en lugar de «no compatible member».
