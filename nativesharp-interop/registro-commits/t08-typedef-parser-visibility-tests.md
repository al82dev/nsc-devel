# Tanda 08: prueba de visibilidad sintáctica de typedef

La prueba preparada inserta `External::CNumber` en el proyecto de aplicación y
comprueba la escritura `External.CNumber` en ese proyecto, sin hacerla visible
en el proyecto que no depende de él. No verifica la resolución semántica del
alias. La ejecución queda para la validación agrupada de la tanda.
