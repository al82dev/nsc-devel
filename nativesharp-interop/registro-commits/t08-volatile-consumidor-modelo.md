# T08-010/015: representación del modo del consumidor

## Alcance de este conjunto

Objetivo: completar el acceso C volatile con un modo del consumidor inequívoco,
incluidos alias, asignaciones, referencias mixtas y compilación precompilada,
sin alterar los accesos nativos. Los cambios y pruebas se realizan en NSC y
sus directorios de trabajo en A:. NSLink, las fuentes LLVM/Clang y la instalación
en C: permanecen protegidos. Se conservan los cambios concurrentes y se
registran cambios independientes en commits separados. La construcción será
incremental y la validación de la tanda, agrupada al completar su implementación.

Se añade a cada `Project` un modo opcional de acceso C `Microsoft` o `Iso`,
distinto de los modos de las referencias a encabezados. El valor ausente no
elige implícitamente ninguno y los constructores existentes lo conservan así.

Este es únicamente el modelo interno: el cargador de proyectos todavía no lo
establece y HIR, MIR y las cachés todavía no lo utilizan. No cambia el código
generado ni resuelve el fallo de alias volatile. Se construirá y validará como
parte del conjunto completo de implementación, no por este cambio aislado.
