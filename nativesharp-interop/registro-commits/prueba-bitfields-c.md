# Campos de bits C con signo y empaquetado

Se prepara un fixture mixto con campos adyacentes con y sin signo, anchura
cero y una estructura `pack(1)`. C inicializa y verifica los valores que
NativeSharp lee y escribe, y se comparan tamaños. La matriz y el IR están
pendientes; no cubre todas las variantes de bitfields o ABI. La primera
ejecución O0 rechazó todos los campos de bits por no encontrarse proyectados;
también detectó comparaciones de `sizeof` con tipos incompatibles, corregidas
mediante conversión explícita. El test aún no se repitió tras esta corrección.
