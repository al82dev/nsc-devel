# Tanda 08: declaraciones, nombres y tipos C

Estado: auditoría inicial; ningún ítem se cierra solo por observar datos en Clang.

- `ClangSession.cpp::describeType` conserva clase, spelling, tipo canónico,
  calificadores y tamaño/alineación cuando Clang dispone de ellos. Modela
  punteros, referencias, arreglos constantes, enums, records y funciones, pero
  no toda esa información se proyecta todavía a NativeSharp.
- `CppScalarTypeMapping.cpp` mapea enteros por ancho y signo, `bool`, `float`,
  `double` y `void`; `CppSemanticFunction.cpp` añade determinados punteros y
  conserva el símbolo y la identidad canónica. Esto no certifica los ABI de
  agregados ni todos los calificadores.
- La captura de records distingue struct, class y union y obtiene campos,
  offsets, tamaño y alineación de Clang. El proveedor semántico de campos
  excluye union; existen barreras para tipos importados por valor. La captura de
  un record no prueba que sea utilizable en código NativeSharp.
- No se encontró un inventario de declaraciones `typedef` ni de valores de
  enumeradores en `src/Interop/Cpp` y sus interfaces. `CppType` identifica
  `Enumeration` y `Function`, pero no representa por sí solo los enumeradores ni
  la firma completa de un puntero a función.
- `ProjectNameLookup` expande aliases NativeSharp mediante `TypeSyntax`; se debe
  decidir cómo conectar aliases extranjeros conservando identidad, ámbitos,
  colisiones y las reglas de acceso, en vez de fabricar equivalencias.

Siguiente acción: completar el diseño de captura y proyección de aliases, enums,
struct/union y punteros a función; implementar las partes y pruebas de la tanda
como conjunto y ejecutar su validación incremental agrupada. Las limitaciones de
caché y del servicio de lenguaje de tandas anteriores siguen vigentes.
