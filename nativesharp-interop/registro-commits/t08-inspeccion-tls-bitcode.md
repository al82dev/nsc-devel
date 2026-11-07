# Tanda 8: detección de TLS en bibliotecas con bitcode

Al seleccionar el enlazador de un proyecto, un objeto LLVM bitcode puede declarar almacenamiento `thread_local` sin contener todavía `_tls_index`: ese símbolo se genera más tarde. La inspección debe reconocer tanto bitcode como COFF, y tratar las entradas que no pueda inspeccionar de manera conservadora.

El cambio pertenece a NSC. NSLink y las fuentes de LLVM permanecen sin modificaciones. La validación incluye bibliotecas ThinLTO y LTO completo, el caso sin CRT que expone la selección real, controles sin TLS y las pruebas ya existentes de la tanda. La selección explícita del usuario conserva prioridad.

La inspección recorre todos los miembros del archivo, incluso los que el enlazador podría no extraer; por ello puede seleccionar LLD de manera conservadora. Un formato ilegible o desconocido tampoco demuestra ausencia de TLS y se trata conservadoramente. Las pruebas y productos temporales se mantienen en A:.
