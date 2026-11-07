# T08-004: proyección de punteros C atómicos

Se amplía la selección para apuntados C bool, record, enum y void si el tipo
puede proyectarse y el almacenamiento tiene tamaño y alineación compatibles.
HIR y las cargas/escrituras LLVM usan el contrato de puntero en lugar de
enumerar los tipos escalares conocidos. Las operaciones compuestas conservan
sus restricciones anteriores. Pendientes construcción y validación dirigida.
