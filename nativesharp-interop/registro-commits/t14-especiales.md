# T14-007, T14-009 y T14-010: punteros a miembro, headers precompilados y módulos C++

## Defecto

En la [auditoría inicial](../TANDA-14-AUDITORIA-INICIAL.md), una llamada calificada a una función con un
puntero a miembro decía `unknown identifier` del namespace, sin motivo. Un encabezado que importa un módulo
lo rechaza Clang.

Causa en el código:

- `describeType` (`ClangSession.cpp`) no distingue un puntero a miembro de otro tipo sin soporte;
- `projectFunction` (`CppSemanticFunction.cpp`) lo rechaza con un motivo genérico, y `CallAnalyzer.cpp`
  solo da el motivo de una función no importada cuando la llamada no es calificada.

## Diseño

- **Punteros a miembro** (T14-007): no tienen tipo NativeSharp. En el ABI de Microsoft su tamaño y su
  representación dependen del modelo de herencia de la clase (uno, dos o tres campos, y ajustes de `this`
  y de base virtual), así que no se proyecta un portador. `CppType::memberPointer` marca `T C::*` y
  `R (C::*)(A)`; una función que los recibe o devuelve no se importa y una llamada, calificada o no, dice
  por qué: `unknown identifier 'T14' in callable/initializer context: C/C++ function 'T14::get' cannot be
  imported: its parameter 'm' of type 'int S::*' is a C++ pointer to member, which has no NativeSharp type`.
  El mensaje conserva el prefijo de identificador desconocido de la forma no calificada, así que cualquier
  función C/C++ no importada (referencia rvalue, retorno de record no admitido) dice ahora su motivo. Un
  local con el nombre del primer segmento oculta el diagnóstico.
- **Headers precompilados** (T14-009): la sesión de NSC analiza el encabezado fuente con la configuración
  del proyecto y nunca lee un `.pch`. Una biblioteca compilada con `-include-pch` se consume desde su
  encabezado; no se mezclan configuraciones porque el `.pch` de la biblioteca no entra en la sesión. Una
  biblioteca que solo distribuye el `.pch`, sin encabezado, no se puede consumir.
- **Módulos C++** (T14-010): la sesión no recibe interfaces compiladas (`.pcm`), así que un encabezado con
  `import` lo rechaza Clang con su diagnóstico (`module 't14' not found`). Para consumirlos, la referencia
  tendría que nombrar cada interfaz y la sesión pasarla a Clang (`-fmodule-file=<nombre>=<ruta>`); la
  interfaz debe haberla producido Clang 23.1.0 con las mismas opciones que la sesión, y su contenido entra
  en la identidad de la sesión y de la caché como un encabezado más. Queda como pendiente documentado.

## Cambio

- `CppType.h`: `memberPointer`.
- `ClangSession.cpp`: `describeType` marca los punteros a miembro.
- `CppSemanticFunction.cpp`: el motivo de una función con un puntero a miembro en el retorno o en un
  parámetro.
- `CallAnalyzer.cpp`: una llamada calificada a una función C/C++ no importada dice por qué.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-14-especiales/validar.py`, con y sin `--dll`:

- `T14.get(&s, T14.pickY())` se rechaza con el motivo; la contraparte `T14.getY(&s)` devuelve 7, igual que
  el control C++ de Clang 23.1.0, O0–O3;
- una biblioteca compilada con `-include-pch` se consume desde su encabezado y devuelve 42, igual que el
  control, O0–O3;
- un encabezado con `import t14;` se rechaza con el diagnóstico de Clang; la contraparte con el encabezado
  clásico pasa.

Con `--dll`, las funciones no inline de los casos positivos están en una DLL.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t14-especiales-fvghptyj` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t14-especiales-h1eah0nl` |

La primera validación agrupada mostró que dos negativos anteriores
(`pruebas-tanda-10-referencias`, `pruebas-tanda-09-sysv-cpp-record-directo/validar-rechazos.py`) buscan el
prefijo `unknown identifier 'T10'`; el mensaje calificado lo conserva y los dos siguen siendo rechazos.

## Límites

- Un puntero a miembro no se puede pasar, devolver ni almacenar desde NativeSharp.
- Los módulos C++ no se consumen.
