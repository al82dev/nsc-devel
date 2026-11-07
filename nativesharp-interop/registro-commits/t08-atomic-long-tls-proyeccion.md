# T08: proyección TLS de long C atómico de 32 bits

El fixture compiló el productor C y NSC rechazó los escalares TLS atómicos
long y unsigned long como no proyectados. Los predicados ya admitían esos
nombres y anchos fuera de TLS, pero exigían `!global.threadLocal` para ellos.

Se elimina esa condición para ambos tipos, conservando signo, ancho y
alineación. Faltan construcción, IR y ejecución O0–O3; no se acredita
aislamiento por hilo ni se modifica NSLink.
