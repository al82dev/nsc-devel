# Tanda 08: ámbito anónimo de typedef

La captura consulta `Decl::isInAnonymousNamespace()` de Clang y conserva el
resultado en el descriptor. El inventario sintáctico utiliza ese dato en vez de
deducir visibilidad de un fragmento del nombre impreso. La representación aún
no resuelve el resto de las reglas de acceso o ámbitos de C++; validación
agrupada pendiente.
