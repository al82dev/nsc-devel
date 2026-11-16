# T18-012 y T18-013: especializaciones de la STL desde NativeSharp

## Comprobación

Este cambio comprueba una biblioteca de plantillas en encabezados; no cambia código de producción.

## Selección (T18-012)

La STL de MSVC del toolset 14.52.36815: plantillas de clase con parámetro no tipo (`std::array`), plantillas
con miembros estáticos (`std::numeric_limits`, `std::integral_constant`) y plantillas de función con
restricciones de tipo (`<bit>`, `<numeric>`), todas definidas en encabezados.

## Evidencia (T18-013)

Fixture nuevo `pruebas-tanda-18-stl/validar.py`. NativeSharp usa `std::array<int, 4>` (`_Elems`, `size()`,
`fill` con `in`, local sin inicializador), `std::numeric_limits<sbyte>::max()`,
`std::numeric_limits<short>::digits`, `std::integral_constant<int, 5>::value`, `std::gcd<int, int>`,
`std::lcm<int, int>`, `std::popcount<uint>`, `std::bit_width<uint>`, `std::countl_zero<uint>`,
`std::has_single_bit<uint>` y `std::rotl<uint>`; un control C++ escribe las mismas expresiones. Resultado
14236 en los dos, O0–O3 (`A:/temp/nsc-t18-stl-hc6rsggv`). La STL no tiene parte binaria aquí: no hay
variante DLL.

## Límites

- `std::min`, `std::max`, `std::clamp`: devuelven `const T&` (retornos por referencia, t10-cierre).
- `std::pair`: su constructor es una plantilla (como `std::unique_ptr`, t15-cierre).
- `std::optional<int>`, `std::string_view` como locales: `experimental C++ local requires direct automatic
  construction and cleanup`.
- `std::midpoint`: la sobrecarga de punteros no se importa y el conjunto queda incompleto.
- `std::array::at`: sobrecargas que devuelven referencia.
