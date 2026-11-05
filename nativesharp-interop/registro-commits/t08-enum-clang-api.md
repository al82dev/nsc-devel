# Tanda 08: tipo canónico de enum en Clang 23

La captura de tamaño y alineación de enum usa `getCanonicalTagType`, declarado
en el `ASTContext.h` de Clang 23.1.0. La llamada anterior a `getEnumType` no
existe en esa interfaz y habría impedido compilar. La verificación es por
inspección de la API; la compilación queda para la validación agrupada.
