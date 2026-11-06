# Sombra local de valores C importados

T08-018: se agregó `ShadowValueLocal.ns` al fixture de sombras existente.
El caso declara dos variables locales con los mismos nombres de un global y
un enumerador C, modifica la variable local y comprueba el global mediante una
función C. Al salir del bloque accede a los nombres C, para detectar tanto una
resolución local incorrecta como una sombra que escape de su ámbito.

El caso quedó incluido en `validar-sombras.cmd` para la validación agrupada.
Está preparado, no ejecutado; su presencia no demuestra todavía que el
compilador resuelva el caso ni cierra T08-018.
