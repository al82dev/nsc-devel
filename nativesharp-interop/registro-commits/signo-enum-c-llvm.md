# Ancho y signo LLVM de enum C importado

La conversión compila pero el ejecutable devolvió 3 al convertir el valor
unsigned `0xffffffff` a `long`: el backend consultaba el tipo por defecto del
enum sintético. Se prepara usar el `enumBase` capturado de Clang para enums C,
dejando la ruta previa de enums NativeSharp. Pendiente de construir y validar.
