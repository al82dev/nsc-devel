# Tanda 08: identidad de agregados C anónimos con typedef

`ClangSession.cpp` conserva el nombre que Clang asocia al typedef de una
estructura o unión C anónima, tanto en el inventario de declaraciones como en
la identidad de los tipos que la referencian. La ruta es exclusiva de C;
agregados anónimos sin typedef siguen sin exponerse como nombres inventados.
La extracción no demuestra todavía uso semántico ni compatibilidad de ABI.
La prueba de la tanda se prepara por separado y se ejecutará con la validación
incremental agrupada; este cambio no se ha compilado ni ejecutado aún.
