# Selección LLVM del global callback volatile

El IR de la primera prueba emitió volatile ordinario en el global pese a
transportar la alineación desde HIR. El selector LLVM solo reconocía claves que
terminan en asterisco, no la identidad estructural de función. Se prepara su
inclusión bajo las condiciones previas de modo MS y alineación. Falta repetir
la construcción y verificar tanto acquire como release en el IR resultante.
La construcción incremental terminó y la comparación de IR comprobó ambos
órdenes para el global; la ejecución mixta pasó O0–O3.
