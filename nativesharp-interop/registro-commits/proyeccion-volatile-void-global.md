# Proyección de globales C volatile void*

El mapeador de globales admite `void` como objeto apuntado volatile inmediato
en modo C, coherente con el soporte existente para parámetros C. Se conserva
el calificador de apuntado y no se modifica la regla que rechaza calificadores
anidados. La ejecución, los diagnósticos y el comportamiento en arreglos aún
requieren la validación agrupada de la tanda.
