# Ancho y signo LLVM de enum C importado

La conversión compila pero el ejecutable devolvió 3 al convertir el valor
unsigned `0xffffffff` a `long`: el backend consultaba el tipo por defecto del
enum sintético. Se prepara usar el `enumBase` capturado de Clang para enums C,
dejando la ruta previa de enums NativeSharp. Pendiente de construir y validar.

Corrección del diagnóstico: Clang Windows confirmó con `_Static_assert` que
el enum inicial sin base fija es signed, por lo que el resultado -1 era
correcto y esa ejecución no demuestra un defecto. El fixture usa ahora base
`unsigned int` explícita (extensión C11 de Clang) y comprueba su signo en C.
La ruta LLVM que usa el `enumBase` importado requiere aún validación propia.

La validación posterior con base unsigned explícita y enum signed verificado
desde C pasó O0–O3. El IR usa `zext i32` para unsigned y `sext i32` para
signed; el control de enum NativeSharp y la suite de sesión también pasaron.
La primera falla no era un defecto: procedía de la expectativa equivocada.
