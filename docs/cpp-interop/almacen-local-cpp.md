# Almacenamiento local para registros C++

La emisión LLVM reserva almacenamiento con el tamaño y la alineación del
registro C++ importado. El código limita esta ruta a locales automáticos no
capturados con un layout válido; los demás casos se rechazan.

Este commit registra un componente experimental; no demuestra por sí solo
construcción, destrucción ni uso completo de objetos C++.
