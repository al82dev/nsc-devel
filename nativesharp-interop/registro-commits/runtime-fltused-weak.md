# Marcador de punto flotante y CRT estático

La prueba TLS con `double` reproduce un símbolo `_fltused` duplicado entre
el runtime de NativeSharp y `libcmt.lib` al usar LLD. El marcador del runtime
se emite con enlace débil para permitir que otro objeto lo proporcione; queda
pendiente validar ambos enlaces, con y sin CRT, y las regresiones pertinentes.
No modifica NSLink.
