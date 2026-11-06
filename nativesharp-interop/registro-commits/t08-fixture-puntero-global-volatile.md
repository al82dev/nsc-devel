# Fixture de puntero global volatile

NativeSharp asigna el global de puntero volatile y C lee, escribe y compara el
puntero. El caso existente sigue verificando que el objeto apuntado se modifica
sin trasladarle el calificador del puntero. Las comprobaciones de IR y la
ejecución O0–O3 se realizarán en la batería agrupada.
