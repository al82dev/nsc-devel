# T08 H3: validar tipos atomicos del codec de dependencias

El nuevo codec interno rechaza un `CppTypeKind::Atomic` sin tipo de elemento tanto al escribir como al restaurar, incluso si esta anidado en otro tipo. La validacion usa el mismo limite de profundidad que el codec y no altera los codecs antiguos.

Se agregaron controles de rechazo de atomico sin elemento y de bytes sobrantes, verificando que el resultado previo no se modifique al fallar. Esta pieza aun no se publica ni consume desde NSMETA. Construccion y ejecucion pendientes para la validacion agrupada; H3 y la tanda 8 siguen abiertos.
