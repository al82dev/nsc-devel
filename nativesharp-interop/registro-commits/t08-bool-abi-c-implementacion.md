# Tanda 8: ABI de bool en funciones externas

Para llamadas externas C/C++ se agrega el atributo LLVM `zeroext` a retornos y parámetros `bool` pasados por valor. Se aplica tanto a la declaración como a llamadas directas de instancia y estáticas; las posiciones se obtienen del ABI físico, incluyendo `this` y el retorno indirecto. Las referencias y punteros no reciben el atributo. La asociación con la función MIR se conserva en la información interna de la declaración, sin cambiar NSMETA ni NSLink.

La construcción incremental de NSC terminó correctamente. La prueba `pruebas-tanda-08-bool-abi-c` contrastó IR con Clang y ejecutó las funciones C en O0–O3: `A:\nsc-interop-build\tanda08-bool-abi-c-14430-12172`. La regresión de globales `_Bool` pasó O0–O3 en `A:\nsc-interop-build\tanda08-bool-global-c-14501-19261`; el control de caché MIR pasó seis comprobaciones.

Estas pruebas cubren llamadas C libres, no acreditan todavía métodos C++, llamadas indirectas, callbacks o atributos de otros tipos. La tanda 8 permanece abierta y no se ha ejecutado su validación completa.
