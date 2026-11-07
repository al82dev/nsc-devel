# Inspección de dependencias NS transitivas

La selección automática recorre las dependencias binarias NativeSharp
registradas en metadata, evita ciclos por ruta de metadata e inspecciona las
dependencias estáticas y las dependencias C de cada módulo. Si una ruta no se
puede inspeccionar junto al módulo, elige LLD conservadoramente: la preparación
normal sigue resolviendo raíces alternativas y verificando identidad y ABI.
No cambia las elecciones explícitas ni NSLink. Pendiente de validación.
