# Long double en biblioteca NativeSharp precompilada

Se prepara una prueba que publica un puntero a struct C con campo long double,
y lo usa desde un consumidor que importa el mismo encabezado. La matriz O0–O3
se agregó al fixture. En O0 el productor generó la biblioteca y su metadata,
pero el consumidor rechazó `CFloatRecord.value` porque no puede reconstruir el
campo. Tras conservar el formato en el catálogo y seleccionar la extensión al
publicar, los casos directo y precompilado pasaron de O0 a O3. La suite de
sesión también pasó; esta evidencia corresponde al destino Windows x64 probado.
