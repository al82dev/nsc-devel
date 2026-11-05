# Tanda 08: correccion de API de contexto Clang

La primera construccion agrupada fallo porque TypedefNameDecl no tiene el
metodo isDependentContext. Se consulta el DeclContext que se esta recorriendo,
que si expone esa API en Clang 23.1.0. Falta reconstruir y ejecutar la bateria.
