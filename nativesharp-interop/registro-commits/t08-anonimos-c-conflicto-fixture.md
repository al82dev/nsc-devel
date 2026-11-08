# Control de definiciones anónimas C incompatibles

El proyecto `Conflict.nsproj` observa el mismo encabezado desde dos sesiones con
preprocesamiento diferente: el campo anónimo `item` es `int` en una y `float` en
la otra. Los tamaños coinciden. El caso permite comprobar que no se consideren
equivalentes solo por compartir archivo y dimensiones.

La ejecución dirigida rechazó el record como definición conflictiva o no
verificada, con código 1. El fixture automatizado exige ese diagnóstico concreto.
La tanda 8 sigue abierta.
