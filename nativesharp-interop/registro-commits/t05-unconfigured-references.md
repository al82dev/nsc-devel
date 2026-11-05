# Tanda 05: referencias sin sesión

Antes de preparar módulos, el controlador rechaza referencias C/C++ si el
compilador no incluye la integración o si no se configuró la raíz de Clang.
Esto evita que una referencia de header pase silenciosamente sin abrir sesión y
ofrece un mensaje específico aun cuando la caché esté activa. La compilación
sin referencias C/C++ conserva el recorrido anterior. Falta validación agrupada.
