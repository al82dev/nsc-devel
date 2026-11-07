# T08-002 H4: consumidor de campo enum nombrado precompilado

El consumidor sin encabezado recibe el record mediante `var`, lee su campo y
lo pasa a un método del productor. Evita el problema T08-020 de declaración
explícita de punteros con metadata. La validación ampliada está pendiente.
