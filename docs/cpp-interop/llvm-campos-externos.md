# Enlace LLVM de campos estáticos C++

Los campos estáticos C++ se declaran como almacenamiento externo, con marca de
importación DLL cuando corresponde. No se emiten los accesores de inicialización
que NativeSharp utiliza para sus propios campos.
