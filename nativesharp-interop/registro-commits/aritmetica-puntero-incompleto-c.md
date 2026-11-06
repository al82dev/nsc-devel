# Aritmética de puntero a record C incompleto

El caso `OpaqueArithmetic.ns` intenta sumar uno a un puntero cuyo tipo C solo
tiene declaración incompleta. Un desplazamiento por elementos necesita el
tamaño del tipo apuntado. La prueba está preparada para comparar el diagnóstico
o IR de NSC con ese requisito.

Con NSC build `bd1f1ac94128ecb4`, `--dump-project-oop-llvm` falló durante el
descenso con `CC1 cannot determine the element layout of
'type:COpaqueArithmetic::Opaque'`. El análisis de aritmética comprueba `void*`,
pero no verifica si el record C apuntado tiene definición completa. El
diagnóstico de backend evita generar código, pero llega tarde y no señala la
expresión como uso inválido de un tipo incompleto.

Se preparó un diagnóstico semántico para `+` y `-` cuando el elemento es un
struct o union C importado incompleto. No afecta `Opaque** + 1`, cuyo elemento
es un puntero con tamaño conocido. Se agregaron controles de ambos casos al
fixture agrupado. Están pendientes la construcción y validación; `+=`,
incremento e indexación utilizan otras rutas.

Después se prepararon comprobaciones separadas para `+=`, incremento e índice
tras reproducir en cada caso un fallo de layout durante el descenso. El script
exige los diagnósticos semánticos de las tres rutas y ejecuta el control
`Opaque**` con aritmética, incremento e índice en O0–O3. Las correcciones y
controles están preparados, todavía no construidos ni validados.
