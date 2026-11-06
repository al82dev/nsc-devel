# T08-002 / T08-004: identidad de globales al seleccionar

Una selección por nombre solo buscaba aliases por símbolo externo. Si otra
observación del mismo proyecto conservaba el mismo USR de Clang con otro nombre
y otro símbolo, quedaba fuera de la comparación de contratos y podía ocultar
una contradicción. La selección incluye ahora también esa observación por USR;
la comparación existente decide si hay conflicto, sin considerar el USR como
prueba de compatibilidad ABI. Se conserva la búsqueda por símbolo y el
aislamiento por proyecto; los USR ausentes no producen coincidencias.

La suite de sesión incorpora casos de contradicción, aislamiento de proyecto
y aliases compatibles con USR distintos. Están preparados para la validación
agrupada de la tanda 08; esta modificación aún no acredita su ejecución ni el
cierre de los ítems.
