# Unidad de almacenamiento de campos C volatile

En Windows x64 con ABI MS, Clang agrupa campos de bits en unidades discretas que empiezan con el primer campo de cada tramo y usan el tamaño de su tipo. Para campos C volatile, la proyección calcula esa unidad a partir de los offsets y tamaños proporcionados por Clang; valida límites, tamaño, alineación y solapamientos. El modelo y LLVM ya transportan el cualificador volatile. Los casos no demostrables se rechazan y la verificación de ejecución e IR se hace en conjunto.
