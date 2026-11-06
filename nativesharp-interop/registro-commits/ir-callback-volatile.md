# Control comparativo de IR volatile

El verificador preparado exige en el IR de Clang y NSC dos cargas acquire y
dos escrituras release alineadas, el acceso específico al global y cargas y
escrituras volatile ordinarias alineadas a uno para el campo empaquetado. La
verificador pasó con los dos IR; el programa mixto también pasó O0–O3. El
control cubre este fixture en Windows x64, no todos los accesos volatile.
