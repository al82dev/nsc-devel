# Tanda 08: proyección inicial de valores de enum C

`projectCppEnumConstant` convierte el texto decimal exacto de Clang a bits y
tipo entero NativeSharp, comprueba el ancho y los límites con y sin signo y
conserva el proyecto. Valores no representables y enums C++ devuelven fallo;
no se truncan silenciosamente. Aún falta publicar el nombre y demostrar su uso
como expresión constante. La validación se agrupará al cerrar la tanda.
