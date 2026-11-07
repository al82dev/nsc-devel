# T08: control mixto de global C `volatile _Bool`

El fixture record-c prepara un global C escalar leído y escrito tanto por
NativeSharp como por C. El validador exige declaración i8, lectura acquire y
escritura release volatile en IR; el programa verifica ambos sentidos durante
la ejecución. El fixture completo se ejecuta en O0–O3.

La implementación y el control aún no se han construido ni ejecutado juntos.
Los controles previos de `_Atomic`, TLS, arreglos y bool nativo se conservan.
No se modificó NSLink.
