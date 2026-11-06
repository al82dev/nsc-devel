# T08-015 y T08-016: proyección de arreglos C volatile

La ruta de enums y records acumula volatile en dimensiones y elemento, lo
conserva en el campo y emplea una copia no calificada para verificar el enum.
La identidad y el layout de records se comprueban como antes. La ruta C++ no
cambia. Los accesos, su alineación y las direcciones siguen sin validar.
