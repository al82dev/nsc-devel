# Protected en tipos genéricos

Cada consulta tiene control ordinario separado. `Owner` espera incluir ambos
campos en el ámbito del dueño; `Outside` espera seleccionar solo el público;
`Derived` explora el acceso protected desde un derivado. La compatibilidad de
herencia genérica y el acceso desde derivados aún deben verificarse: un rechazo
conservador explícito no equivale a soporte, y un fallo del control ordinario
impide atribuir el resultado a `Fields`.
