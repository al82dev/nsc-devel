# Propagación de NS_LIB_ROOT en nsc build

`nsc build` conserva la raíz de respaldo absoluta del entorno en las opciones
usadas para cargar y preparar la clausura. Una raíz explícita o del proyecto
mantiene prioridad; una variable vacía o relativa se ignora como en el driver
directo. La corrección no cambia NSLink ni la validación de metadata.
