# Callback C a través de biblioteca NativeSharp precompilada

Se preparó productor que publica una firma de callback C y un puntero a
struct C con campo callback, y consumidor que importa el mismo encabezado,
llama el callback retornado y lee e invoca el campo. Pendiente de validación
agrupada O0–O3 y reconciliación de metadata. La matriz se detuvo en O0:
el productor generó `.nsmeta`, pero el consumidor no encuentra el método
`Published.Bridge.GetCallback`. El nombre aparece en el archivo de metadata;
esto no demuestra que su firma pueda reconstruirse. Los niveles O1–O3 y el
caso de campo de ese consumidor no se ejecutaron.
