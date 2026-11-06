# Conversión numérica desde enum C

El proyecto real rechazó enum C hacia int, long y otro enum C. Se prepara la
clasificación numérica para el origen enum C importado y destino integral o
enum C, sin incluir enums NativeSharp. El lowering debe usar el ancho y signo
del enum de origen. Pendiente validación agrupada y control negativo NS.

Con `2d0b3a78ed4e6747` la conversión a int, entre enums C y a long ejecutó de
O0 a O3. El enum NativeSharp conservó su rechazo. Clang confirmó mediante
`_Static_assert` las bases signed y unsigned de los casos; el IR de NSC usa
`sext` y `zext` respectivamente. La suite de sesión pasó 1/1. No cubre todas
las operaciones aritméticas o valores de todos los enums C.
