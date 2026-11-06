# Alcance del puntero C volatile int* directo

La referencia Clang demuestra acquire/release para el apuntado int. Se acota
la primera corrección a una desreferencia cuyo operando HIR es directamente un
global C importado con modo MS conocido. Otros orígenes precisan transporte de
procedencia y permanecen pendientes. Los controles se ejecutarán con la tanda.
