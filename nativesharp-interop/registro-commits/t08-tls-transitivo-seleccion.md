# Selección de LLD para TLS transitivo

La selección automática consulta las dependencias C/C++ declaradas en la
metadata de las bibliotecas precompiladas e inspecciona las estáticas antes
de identificar la caché. Si no puede leer o resolver esa información, elige
LLD conservadoramente y deja el diagnóstico a la validación habitual. Aplica
a `nsc build` y a emisiones directas y respeta selecciones explícitas. La
validación se efectuará por tanda; esta corrección no cierra TLS de DLL.
