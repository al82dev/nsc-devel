# Tanda 8: control de bitcode sin TLS

El validador compila el ejemplo C sin TLS como ThinLTO y como LTO completo, verifica que su IR carece de `thread_local`, exige selección automática de LLD y ejecuta el programa. Ambas variantes pasaron en el compilador de desarrollo de A:. El control existente con COFF nativo sigue exigiendo NSLink.
