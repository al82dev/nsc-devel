# T08 H3: codec interno para enumeradores anónimos

El catálogo C de tipos solo transporta enums nombrados. Se añade al catálogo interno de dependencias una entrada para la identidad del enumerador, su módulo, referencia, tipo y bits. Cuando existen esas entradas se usa la versión 2 del catálogo interno; sin ellas, la codificación conserva la versión 1 y sus bytes canónicos.

La construcción incremental terminó con éxito y `nsc_cpp_c_identity_tests` pasó. El control verifica ida y vuelta de una entrada de enumerador y la versión 1 sin entradas nuevas. Esta preparación no captura ni publica enumeradores anónimos desde un productor, ni acredita su consumo: siguen pendientes la integración, la política de versión NSMETA y las pruebas de extremo a extremo.
