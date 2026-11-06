# Codec interno del catálogo de records C

Se codifican y recuperan dimensiones, categorías, campos y árboles recursivos
de tipos, incluidos calificadores e identidades. La lectura limita cantidades,
profundidad y valores de indicadores, y rechaza datos truncados o sobrantes;
la salida del llamador se sustituye solo al terminar. Este codec todavía no se
incorpora a NSMETA ni sustituye la validación semántica del catálogo.
