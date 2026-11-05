# Acción privada de sesión Clang

La acción crea el AST con `ASTUnit::LoadFromCompilerInvocation` y mantiene el
consumidor de diagnósticos como referencia prestada. La invocación usa un archivo
virtual superpuesto y coloca la capa cancelable alrededor de ese sistema.

El ciclo de frontend se delega a `ASTUnit` de Clang 23.1.0. El resultado y sus
condiciones de apertura y cierre se verificarán con la tanda completa.
