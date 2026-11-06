# T08-015: diagnóstico preciso de dirección volatile

El control buscaba solo `volatile`, texto que podría aparecer en el nombre de
un archivo o caso. Los logs observados contienen el diagnóstico específico de
dirección de almacenamiento volatile; el script ahora exige esa frase en los
cuatro casos. Falta repetirlo con el control corregido.
