# Identidad de tipos anidados en campos

La comparación recursiva de tipos record y enum verifica USR conocidos. La
selección compara cada definición con las anteriores para que una observación
sin USR no oculte un conflicto entre las demás. No acredita persistencia ni
equivalencia ABI completa; está pendiente de validación agrupada.
