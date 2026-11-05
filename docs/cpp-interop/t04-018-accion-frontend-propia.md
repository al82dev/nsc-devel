# T04-018: acción de frontend de sesión

La acción de herramientas crea una acción propia basada en `SyntaxOnlyAction` y
la entrega a `ASTUnit::LoadFromCompilerInvocationAction` con persistencia del
AST. El `FileManager` de la unidad utiliza el sistema de archivos cancelable de
la sesión, conservando la superposición de entrada virtual.

La prueba agrupada debe confirmar que se conservan Sema, consultas, diagnósticos,
cancelación y ciclo de apertura/cierre antes de marcar el ítem.
