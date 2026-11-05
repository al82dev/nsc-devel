# Tanda 08: comprobación de global enum volatile

- El proyecto escribe y lee `volatile_mood` desde NativeSharp y compara el valor con una lectura C. El script exige además un `load volatile` y un `store volatile` del símbolo en el IR generado.
- Estado: preparado para validación agrupada. No demuestra punteros a volatile ni todas las optimizaciones.
