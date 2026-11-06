# Redeclaraciones de tipos C opacos

El encabezado de la prueba incluye una declaración adelantada adicional para
el struct y la union, sin cambiar sus definiciones privadas ni su API. Es C
válido y debe conservar la identidad entre el productor precompilado y el
consumidor que incluye el mismo encabezado. Falta validar y diagnosticar el
resultado; no acredita el caso todavía.
