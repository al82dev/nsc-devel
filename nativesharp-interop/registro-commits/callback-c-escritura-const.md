# Escritura y const de callbacks C almacenados

Se preparó escritura desde NativeSharp de callbacks en campo y global C, con
invocación desde C, y lectura de punteros const. Dos proyectos independientes
comprueban el rechazo de escritura en campo y global const. La prueba mixta
pasó O0–O3 y los dos controles negativos se rechazaron con el diagnóstico de
destino no asignable. No acredita semántica volatile ni otras firmas.
