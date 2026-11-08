# T08-010/015: diagnóstico de aritmética de alias C volátil

El control compara lectura y escritura `*(data + index)` sobre un alias de
arreglo flexible C con Clang, además de los accesos directo, indexado y
reasignado existentes. `diagnosticar-var-volatile.cmd` terminó con código 1:
en O0–O3 el modo Microsoft de Clang usa acquire/release y NSC emite acceso
volátil sin orden; en modo ISO los dos nuevos casos coinciden. La comparación
de referencias mixtas también difiere respecto al modo Microsoft.

El análisis de aritmética de punteros conserva el contrato C del alias, pero
restablece la alineación conocida a 1; esa pérdida debe contemplarse al
implementar el contrato de acceso. El control demuestra el desacuerdo de IR,
no demuestra que esa pérdida sea la única causa: el modo por consumidor aún
está protegido y no está disponible en la generación normal de código.
