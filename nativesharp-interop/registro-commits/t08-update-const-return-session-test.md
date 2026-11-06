# Expectativa de sesión para retorno const

La primera batería mostró que la suite todavía clasificaba `tools::pointer`,
que devuelve `const int*`, como no admitida. La prueba exige ahora la proyección
y la conservación del calificador capturado por Clang, y mantiene el rechazo de
los casos interno y de record incompleto. Falta repetir la batería agrupada.
