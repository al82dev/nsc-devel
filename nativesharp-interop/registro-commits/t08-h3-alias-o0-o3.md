# Tanda 8: alias de plantilla en O0–O3

El fixture de biblioteca estática acepta `NSC_T08_OPT` para elegir el nivel de
optimización del productor y del consumidor. Se ejecutó en O0, O1, O2 y O3:
las cuatro ejecuciones compilaron la biblioteca, inspeccionaron el alias en
NSMETA 1.46, compilaron el consumidor sin el encabezado y retornaron cero.

Esta evidencia cubre el caso escalar del fixture. Quedan pendientes las
colisiones de nombres, los demás tipos de alias, DLL, funciones y globales y
la validación completa de la tanda 8.
