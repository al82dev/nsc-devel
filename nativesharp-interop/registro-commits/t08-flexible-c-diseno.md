# T08-010: miembro de arreglo flexible C (`T data[]`)

## Estado

PREPARADO, NO APLICADO. La implementación está escrita, construida y probada en la carpeta de compilación de
la rama, pero no se integra en el código porque contradice un contrato de prueba existente (ver "Bloqueo").
El parche exacto queda en [t08-flexible-c-implementacion.patch](t08-flexible-c-implementacion.patch).
No modifica NSMETA ni el codec del catálogo C.

## Representación elegida

- Clang ya marca el campo como arreglo de límite desconocido (`CppType::isIncompleteArray`) y el catálogo C de
  NSMETA ya lo persiste (`CppCRecordMetadataCodec.cpp`, `type.isIncompleteArray`); la validación del catálogo
  acepta un campo sin tamaño cuyo offset es igual al tamaño del record. HECHO COMPROBADO por lectura y por la
  ejecución del consumidor sólo-metadata.
- NativeSharp ya tiene arreglos flexibles nativos `T[*]` (tamaño cero, sólo como último campo de un struct que
  no es union, índice sin límites salvo `[TrailingStorage]`), y el generador de headers C ya traduce `T[*]` a
  `T name[]`. El campo C se proyecta como `T[*]`: no se inventa semántica nueva.
- Offset: el de Clang. Tamaño de almacenamiento: cero. Alineación: la natural del elemento.
- El camino precompilado proyecta los campos reconstruidos del catálogo con la misma función
  (`CppSemanticRecord.cpp`, `reconstructPrecompiledCMembers`), así que directo y precompilado quedan iguales.

## Cambios del parche (3 archivos, 73 líneas añadidas)

1. `src/Interop/Cpp/CppSemanticRecord.cpp`: `projectCFlexibleArrayField` y su llamada desde
   `projectCppSemanticInstanceField` sólo para campos con `isIncompleteArray`.
2. `src/Frontend/ProjectMemberModel.cpp`, `setExperimentalCppInstanceFields`: acepta tamaño cero con alineación
   no nula sólo para claves `T[*]`; las demás claves conservan la regla anterior.
3. `src/Frontend/ProjectObjectLayoutModel.cpp`, layout de campos importados: si la clave es `T[*]`, usa cero
   bytes y la alineación del elemento.

## Restricciones (el campo sigue sin proyectarse, como antes)

- records C++ y unions (C no admite el miembro flexible en una union);
- miembro que no es el último campo;
- arreglo o elemento `const`/`volatile` (el `T[*]` nativo exige elemento sin calificar);
- elemento que es arreglo, tipo incompleto o algo distinto de escalar, enum o record C completo importado;
- offset que no conserva la alineación natural del elemento (records empaquetados).

## Pruebas (`nativesharp-interop/pruebas-tanda-08-flexible-c`)

- Elementos `int`, `double`, record, enum y `_Bool`; `sizeof`, `alignof` y `offsetof` contra C; escritura
  desde NativeSharp leída por C, escritura de C leída por NativeSharp y dirección de elemento `&p->data[i]`.
- Directo, precompilado con encabezado y sólo-metadata, O0–O3.
- Alineación IR contra Clang con `verificar_alineacion_clang.py` y `tabla-alineacion.txt`.
- Negativos: `const int data[]` y miembro en `pack(1)` deben seguir rechazados.
- Sondas que sólo registran: `sizeof(p->data)`, conversión implícita a puntero y retorno por valor.

## Resultados con el parche aplicado (build `333a5bfa63454b7e`, log `flexible-3.log`)

- Directo, Shared y Metadata pasaron O0–O3.
- IR: `int`, `double`, record y enum IGUAL en lectura y escritura en los tres caminos. `_Bool` queda como
  informativo: NSC usa `i1` y Clang `i8`. Esa diferencia es previa y general: el validador de layout existente
  ya emite `load i1`/`store i1` para el campo y el arreglo `_Bool` de `CBoolFields` (`project.ll` de
  `pruebas-tanda-08-layout`). Pertenece a T08-006 y no se cambia aquí.
- Negativos `RestrictedConst` y `RestrictedPacked`: rechazo confirmado.
- Regresión con el parche: `pruebas-tanda-08-layout` (copia aislada) O0–O3 con negativos, y
  `pruebas-tanda-08-layout-clang` completo, pasaron (`layout-base-2.log`, `fixture-clang-8.log`); la sonda
  `ProbeFlexData` del registro anterior pasó a compilar y ejecutar con código 0.
- Sondas: `sizeof(p->data)` se rechaza ("it has no value layout"), igual que C rechaza `sizeof` de un tipo
  incompleto. La conversión implícita `int* first = p->data;` se rechaza; C la permite. Se accede con
  `&p->data[0]`. Cambiarlo sería una conversión nueva del lenguaje para `T[*]`: no se implementa.

## Estado sin el parche

Con las fuentes restauradas (build `7fb3fcdc61f265b4`, log `flexible-sin-parche.log`) el fixture falla en el
primer paso, como corresponde: `receiver/type 'type:CFlexibleArray::FlexInts' has no compatible member named
'data'`. `validar.cmd` es la prueba de aceptación del parche y hoy termina con 1.

## Bloqueo

`tests/unit/CppClangSessionTests.cpp`, líneas 4888–4892, afirma literalmente el contrato anterior:

```cpp
const auto flexibleField = flexible ? projectCppSemanticInstanceField(*flexible, 1, 0, "Semantic", ...) : ...;
ok &= expect(matrixField && ... && !flexibleField,
             "C array field retains fixed dimensions and rejects incomplete bounds");
```

Con el parche, `struct Flexible { int count; int values[]; }` proyecta `values` y esa aserción falla
(INFERENCIA por lectura del código; la suite no se pudo ejecutar en esta rama porque sus fixtures sólo existen
bajo `A:\nsc-interop-build\cpp-fixtures`). Según los lineamientos, una prueba existente sólo cambia con un
reemplazo de contrato autorizado explícitamente, y ese archivo es compartido con Codex. Hace falta:

1. autorización para reemplazar el contrato "rechaza límites incompletos" por "proyecta el miembro flexible
   final como `T[*]`";
2. cambiar esa aserción para exigir `flexibleField->type.nativeSharpTypeKey == "builtin:int[*]"`, offset 4,
   tamaño 0 y alineación 4, coordinándolo con Codex;
3. aplicar el parche, reconstruir y ejecutar este fixture y la suite de sesión.

## Hallazgo no autorizado: retorno por valor de structs con miembro flexible (tanda 9)

Clang 23.1.0 devuelve por `sret` cualquier struct con miembro flexible en Windows x64
(`define void @flex_ints_value(ptr sret(%struct.FlexInts) ...)`), aunque mida 4 bytes. NSC declara
`i32 @flex_ints_value()` y, para el caso empaquetado que el parche no proyecta, `i8 @flex_packed_value()`.
Las dos sondas compilan y terminan con 0xC0000005. El defecto existe sin el parche: la clasificación de
retorno de NSC ignora el miembro flexible. Pertenece a T09-007/T09-008 (agregados por valor conforme al ABI),
que no se tocan por orden de no avanzar a la tanda 9.
