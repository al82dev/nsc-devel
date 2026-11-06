# Asignación compuesta bool: controles de frontera

La inspección de `AssignmentAnalyzer.cpp::finishNumericCompoundAssignment`
y `ProjectTypeConversion.cpp::commonNumericType` indica que el compilador
rechaza bool como operando numérico de asignación compuesta. Se prepararon
dos controles negativos, uno sobre el arreglo C y otro sobre un bool NS,
para comprobar esa frontera sin atribuir compatibilidad. Aún no se ejecutan;
la posibilidad de ampliar el lenguaje permanece pendiente de análisis.
