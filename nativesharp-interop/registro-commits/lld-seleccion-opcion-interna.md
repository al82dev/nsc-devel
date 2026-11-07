# Opción interna para selección de enlazador

El analizador de la invocación del compilador conserva una selección explícita
`NSLink` o `LLD` y rechaza valores inválidos o duplicados. El campo es por
invocación y no modifica variables de entorno. Esta pieza todavía no está
conectada al comando `build` ni al despacho; no altera la selección efectiva
del enlazador. La validación se hará junto con la propagación completa.
