# Tanda 08: combinación de enumeradores y variables importadas

`provideCppSemanticValues` combina los candidatos solicitados de variables y
enumeradores. Si coinciden en proyecto, namespace y nombre, conserva un solo
candidato con error diferido para que la resolución no elija arbitrariamente un
valor. Esta entrada aún no sustituye al proveedor del driver; faltan pruebas y
validación conjunta antes de activarla.
