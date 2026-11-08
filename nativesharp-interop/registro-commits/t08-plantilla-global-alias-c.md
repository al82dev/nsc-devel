# Global y typedef C requeridos por plantillas

`plantillas/validar-global-alias-o0.cmd` reutiliza el fixture existente. Genera
la biblioteca C y la biblioteca NativeSharp, retira la copia temporal del
encabezado y compila y ejecuta dos consumidores: uno usa un typedef C dentro
de una plantilla y el otro escribe y lee una global C. Ambos pasaron en O0.
La biblioteca C permanece disponible para enlazar. Es una muestra dirigida y
no acredita variantes adicionales ni la batería completa del bloque.
