# Alcance del codec de records C

La metadata necesita transportar las observaciones sin depender de Clang en
tiempo de lectura. Se prepara un codec interno de tipos y records C con límites
y rechazo de datos inválidos. La versión del formato y la publicación no se
cambian hasta completar también huella ABI, validación, fusión y reconstrucción.
