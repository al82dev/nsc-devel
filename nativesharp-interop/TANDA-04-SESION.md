# Tanda 04: implementación de sesión Clang

Estado: implementación preparada; **validación agrupada pendiente**. No se
considera cerrada la tanda ni se marcan sus ítems por la mera presencia de código.

La sesión separa la construcción de argumentos, la captura de diagnósticos y la
acción de Clang. La acción propia es un `ToolAction` que utiliza `ASTUnit` de
Clang 23.1.0; este crea internamente su acción de frontend y administra la
apertura y cierre de la unidad. Debe contrastarse el alcance literal de
T04-018 con esa decisión, además de verificar los caminos de éxito y error.

El sistema de archivos consulta el token de cancelación en operaciones que
pueden interceptarse. Esto no garantiza interrupción inmediata durante trabajo
de CPU ni en una operación de E/S ya iniciada. Las pruebas añadidas verifican
los argumentos y la cancelación antes y después de consultas al sistema de
archivos; queda por comprobar su funcionamiento junto con la sesión real.

La construcción incremental ON/OFF y las pruebas dirigidas de sesión se deben
ejecutar juntas tras completar la implementación y corregir sus resultados como
grupo. Las pruebas de sobrecarga de dos punteros siguen siendo trabajo de otra
tanda y no se consideran aprobadas por esta validación.
