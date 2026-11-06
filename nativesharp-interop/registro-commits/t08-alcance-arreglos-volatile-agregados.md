# T08-015 y T08-016: alcance para arreglos C volatile

La ampliación prepara volatile para arreglos de enum y record por valor en
campos C, sin modificar C++, LLVM ni NSLink. Deben preservarse calificadores,
alineación y accesos volatile en los controles agrupados.
