# T14-006 y T14-008: bit-fields y atributos de layout en records C++

## Defecto

En la [auditoría inicial](../TANDA-14-AUDITORIA-INICIAL.md), los bit-fields de un record C++ y el campo
subalineado de un record `[[gnu::packed]]` no eran miembros (`no compatible member named 'a'`); en C, los
mismos records funcionaban.

Causa en el código (`CppSemanticRecord.cpp`):

- la proyección de un bit-field exigía `!record.isCxx`, aunque en el ABI de Microsoft el layout de los
  bit-fields de C y C++ es el mismo y Clang da sus coordenadas en ambos;
- un campo con alineación efectiva menor que la natural solo se aceptaba en records C.

## Diseño

- **Bit-fields** (T14-006): en un struct o union C++, un bit-field público entero o bool se lee y escribe
  con el offset y el ancho que da Clang (`ASTRecordLayout`), como en C. Las ventanas de acceso salen de esas
  coordenadas; un bit-field de ancho cero solo separa unidades.
- **Atributos de layout** (T14-008): `[[gnu::packed]]`, `#pragma pack`, `alignas`, `__declspec(align)` y
  `[[msvc::no_unique_address]]` cambian el layout que calcula Clang, y NSC usa ese layout. Un escalar o
  puntero subalineado de un record C++ empaquetado se accede con la alineación efectiva, como en C.
  `[[no_unique_address]]` no tiene efecto en el ABI de Microsoft. Los atributos que cambian una llamada
  (convenciones de llamada, `preserve_none`, `regcall`, `sysv_abi`) los cubre la tanda 9.

## Cambio

- `CppSemanticRecord.cpp`: los bit-fields de records C++ se proyectan como los de C; un campo subalineado
  de un record C++ empaquetado se accede como en C.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-14-registros/validar.py`, con y sin `--dll`. En `namespace T14`: bit-fields
con y sin signo que comparten unidad, uno con signo negativo y uno tras un bit-field de ancho cero; un
record `[[gnu::packed]]`, uno con `#pragma pack(1)` y un `double`, uno `alignas(16)`, uno
`__declspec(align(32))` y uno con `[[msvc::no_unique_address]]`. NativeSharp escribe los campos, la
biblioteca los lee y NativeSharp los vuelve a leer; los `sizeof` coinciden con los de C++. Resultado 76922
frente al control C++ de Clang 23.1.0, O0–O3, con las 4 llamadas C++ de NSC iguales a las de Clang.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t14-registros-slok2qz2` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t14-registros-a6ck6270` |

## Límites

- Un bit-field de tipo enum o mayor que 64 bits no se proyecta.
- Un bit-field privado o protegido no es accesible, como cualquier campo no público.
