# Emisión aislada de funciones C static inline

La sesión solicita a CodeGen de Clang una unidad LLVM para varias funciones C static inline, con nombres de exportación suministrados por el llamador. Las funciones seleccionadas comparten módulo y se valida el IR. Se rechazan nombres duplicados, declaraciones ausentes y sesiones C++; el llamador aún debe asignar identidades, planificar y enlazar el objeto. La emisión aislada no constituye interoperabilidad completa.
