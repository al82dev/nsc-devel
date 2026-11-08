# T08 H3: aclaración de identidad de enumeradores anónimos

La revisión del fixture `plantillas/casos.txt` y de
`t08-h3-enum-anonimo-archivo-identico.md` confirma que `GenIdentical.h` tiene
los mismos bytes que `Gen.h`, pero es un archivo distinto. La matriz espera
ambigüedad para esa copia y ejecución para el mismo archivo compartido. No se
trata el contenido idéntico como prueba suficiente de identidad, porque los
USR de enumeradores anónimos pueden colisionar entre fuentes distintas.

Esta actualización resume evidencia existente; no modifica el compilador ni
repite la matriz. No cierra T08-002.
