# T08-006: globales C bool no volátiles

Clang y NSC declararon y accedieron al escalar y arreglo de _Bool como bytes.
La lectura y escritura cruzada entre ambos lenguajes pasó O0–O3. La prueba se
ejecutó mediante CALL para verificar correctamente su estado. Evidencia:
`A:\nsc-interop-build\tanda08-bool-global-c-12265-29403`.

Pasaron los controles de bool volatile precompilado y DLL, globales atómicas y
contratos de globales internas. También pasaron las pruebas de identidad LLVM
(15 comprobaciones) y restauración MIR focalizada (6), incluidas las marcas
nuevas. Se construyó incrementalmente el compilador y los dos objetivos de
prueba; la construcción presentó dos advertencias previas de inicialización.

No se acredita el tratamiento de bool C a través de punteros escapados,
campos C de la otra rama, todas las formas multidimensionales ni consumo de
declaraciones C desde plantillas públicas. El ítem y la tanda siguen abiertos.
