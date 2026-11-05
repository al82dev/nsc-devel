# Tanda 08: global enum C volatile

- La proyección valida identidad y layout sobre una copia sin `volatile` de nivel superior y conserva el calificador original para el acceso al global. No se cambia el helper compartido ni el tratamiento de punteros a volatile.
- Se prepara un proyecto que lee y escribe el global y comprueba operaciones LLVM `volatile` sobre el símbolo exacto. Pendiente de validación agrupada.
