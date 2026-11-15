# T10-016: operadores C++ libres sobre expresiones NativeSharp

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md), `a + b` y `a == b` sobre records C++
fallaban con `binary operator requires compatible numeric operands`: una función `operator+` libre
de C++ no llegaba a NativeSharp.

Causa en el código:

- `CppSemanticFunction.cpp` (`projectFunction`) proyectaba `operator+` como una función de namespace
  ordinaria con ese nombre. La búsqueda de operadores de NativeSharp (`stage38OperatorCandidates`,
  `MemberAccessAnalyzer.cpp`) solo consulta métodos de operador (`op_Addition`) y operadores de
  extensión (`extensionFunctionsNamed`).
- Una expresión binaria o unaria no genera demanda de función C++
  (`ProjectNamespaceFunctionModel.cpp`, `walkNamespaceNode`): solo los nombres y las llamadas la
  generan. La función operador no se seleccionaba.
- En C++ los operadores se buscan en los namespaces asociados a los tipos de los operandos. La
  selección por demanda (`selectCppSemanticDemands`) solo usaba los namespaces visibles desde el uso.

## Diseño

Una función operador libre de C++ es un operador de extensión NativeSharp:

- Su nombre NativeSharp es el método de operador de su símbolo y aridad
  (`language::operatorMethodName`): `operator+` con dos parámetros es `op_Addition`, `operator-`
  con uno es `op_UnaryNegation`. El símbolo de enlace sigue siendo el de Clang.
- Resuelve NativeSharp, con sus reglas de operadores y de sobrecarga, como un operador de extensión
  escrito en NativeSharp.
- Se proyecta solo si:
  - sus operandos se pasan por valor o por `const T&`, que pasa a ser un operando `in`, como los
    operadores NativeSharp;
  - al menos un operando es un record o enum C++ importado (no un puntero ni un arreglo);
  - no devuelve una referencia.

  Si no, se rechaza con diagnóstico y el operador no se aplica.
- Se busca como en C++, en los namespaces asociados: un operador se considera si está declarado en
  el namespace de uno de sus propios operandos record o enum. Un operador de otro namespace no se
  aplica aunque sus tipos coincidan.

Los operadores miembro (`V::operator+`) son métodos de instancia y continúan en la tanda 11.

## Cambio

- `ProjectNamespaceFunctionModel.cpp`: una expresión binaria o unaria con un operador que tiene
  símbolo C++ (`cppOperatorSpelling`) genera la demanda `operator<símbolo>`.
- `CppSemanticFunctionGroups.cpp`:
  - `cppDemandNamespaces` añade a una demanda de operador los namespaces de las declaraciones del
    mismo operador asociadas a sus operandos;
  - la materialización y el proveedor precompilado aceptan esas demandas por nombre.
- `CppSemanticFunction.cpp` (`projectFunction`): nombra el operador por su método, lo marca como
  extensión y comprueba las condiciones de operandos y retorno.
- `ProjectCompilationSnapshots.cpp`:
  - la validación de funciones diferidas acepta una demanda de operador por el nombre de la
    declaración;
  - una demanda de operador pertenece a la plantilla genérica cuyo cuerpo contiene la expresión
    (índice de padres de la plantilla). Una expresión de operador no es un nombre externo de la
    plantilla, y sin esto el catálogo de una plantilla precompilada no publicaba el operador.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-10-operadores/validar.py`, con y sin `--dll`. Encabezado en
`namespace T10`:

- `struct V { int x; int y; }`;
- por valor: `operator+`, unario `operator-`, `==`, `!=`, `<`, `>` y `operator*(V, int)`;
- por `const V&`: el binario `operator-`;
- `enum class Flag : unsigned` con `operator|`.

El consumidor escribe `a + b`, `-a`, `a == a`, `a != b`, `a < b`, `b > a`, `a * 3`, `b - a` y
`T10.Flag.A | T10.Flag.C`. De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado 53);
- exige las 9 declaraciones del IR de NSC con los tipos físicos de Clang (el operando `const V&` es
  `ptr` en ambos);
- consume una plantilla precompilada que usa los operadores, con y sin encabezado.

Controles negativos, con el diagnóstico `binary operator`:

- `Other::operator%(T10::V, T10::V)`, de un namespace no asociado, no se aplica;
- `operator^(V&, V&)`, con operandos por referencia mutable, no se proyecta.

La contraparte `a + b` del mismo encabezado negativo sí se aplica.

La versión anterior del fixture rechazaba `operator^(const V&, const V&)`. Ese negativo quedó
obsoleto con los operandos `in`: el caso `const V&` pasó a la comparación positiva contra Clang y
el rechazo genuino usa `V&`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-operadores-p35o7riv` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-operadores-yo_s6g5s` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Operadores miembro: tanda 11, con los métodos de instancia.
- Operadores con operandos `T&` o `T&&`, o que devuelven una referencia (como `operator+=(V&, V)` o
  `operator<<` de flujo): se rechazan.
- Un operador C++ sin método de operador NativeSharp para su aridad (por ejemplo `operator,` o
  `operator new`) se rechaza.
- Como en un operador NativeSharp con operandos `in`, un operando temporal (`a + b + a` con
  `const V&`) no se acepta todavía: la sonda `sonda-ns.py` muestra el mismo rechazo con un operador
  escrito en NativeSharp (`A:/temp/nsc-sonda-ns-p59w1vm6`, caso `op-in-rvalue`).
- Operadores de plantillas de función C++: tanda 13.
