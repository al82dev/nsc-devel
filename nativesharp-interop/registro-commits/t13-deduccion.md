# T13-005 y T13-014: llamadas a plantillas de función C++ con argumentos deducidos

## Defecto

Tras el [cambio 2](t13-plantillas-ns.md), `T13.twice<int>(21)` funcionaba, pero `T13.twice(21)` seguía sin
candidato: la sonda de la auditoría daba `unknown identifier`.

Causa en el código:

- la deducción necesita los tipos de los argumentos, y NativeSharp los conoce solo al analizar los cuerpos;
- el modelo de funciones de namespace se construye antes de los cuerpos y el analizador de cuerpos es
  `const`: no puede añadir candidatos mientras analiza;
- `collectFreeFunctions` solo importa las especializaciones pedidas con argumentos explícitos.

## Diseño

- **Sitios de llamada**: al construir el modelo de funciones de namespace, cada llamada directa cuya
  búsqueda de nombre (con sus namespaces y `using`) alcanza una plantilla de función C++ de un proyecto
  visible se marca con los nombres C++ calificados que puede nombrar. Un local o miembro con el mismo nombre
  la oculta.
- **Primer análisis**: en esos sitios el analizador analiza los argumentos y registra su tipo
  (`ref`, `out` e `in` pasan su operando como lvalue). La llamada queda pendiente.
- **Deducción** (T13-005, T13-014): cada llamada registrada se ofrece a las sesiones del proyecto con el tipo
  C++ de cada argumento (tipo básico, puntero, record o enum importado por su identidad de Clang); un escalar
  es un prvalue y un record un lvalue, como una variable NativeSharp. `Sema::DeduceTemplateArguments` con
  los argumentos de la llamada deduce cada plantilla con ese nombre, paquetes incluidos, y comprueba sus
  restricciones; Sema instancia la definición de cada especialización deducida. Las especializaciones
  deducidas se importan con el nombre de su plantilla, como sobrecargas.
- **Segundo análisis**: el modelo de funciones de namespace se construye otra vez y los cuerpos se analizan
  otra vez; los diagnósticos del primer análisis se descartan y se conservan los de las demás fases. La
  resolución de sobrecarga de NativeSharp elige entre las especializaciones deducidas y las funciones que no
  son plantilla; como en C++, si las conversiones empatan, la que no es especialización de plantilla es
  mejor (`over(5)` llama a `over(int)`, `over(5.5)` a `over<double>`).
- Una deducción que Clang rechaza se informa en la llamada, con las notas de Clang, solo si no hay otra
  función que llamar ahí.

## Cambio

- `CppInteropSession.h`: `deduceFunctionTemplateCall`; `CppTemplateArgument::lvalue`.
- `ClangSession.cpp`: `deduceFunctionTemplateCall`; la especialización importada se marca
  (`CppFunctionDeclaration::templateSpecialization`).
- `ProjectNamespaceFunctionModel.h/.cpp`: `CppTemplateCallSite`, `CppTemplateDeductionRequest` y el
  registro de deducciones, que el analizador escribe desde sus hilos.
- `ProjectCompilationSnapshots.cpp`: los sitios de llamada al construir el modelo de funciones de namespace.
- `Bodies/CallAnalyzer.cpp`: registro de los tipos en `analyzeNamespaceCallHead`; preferencia por la
  función que no es especialización en `selectNamespaceFunctionCandidate`.
- `ProjectCompilation.h/.cpp`: `deduceCppTemplateCalls`, `buildNamespaceFunctions` y el segundo análisis
  en `compileProjectMethodBodies`.
- `NamespaceFunctionSymbol.h`, `CppFunctionDeclaration.h`, `CppSemanticFunction.cpp`: la marca
  `cppTemplateSpecialization`.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-13-deduccion/validar.py`, con y sin `--dll`: `twice(21)`, `twice(1.5)`,
`maxOf(4, 9)`, `maxOf(3.5, 2.0)`, `sizeOfPtr(&x)` (`const T*`), `sumPoint(in p)` (`const T&` con un record),
`countAll(1, 2, 3)` (paquete), `over(5)` y `over(5.5)` (plantilla sobrecargada con una función que no lo es)
y `small(3)` (restricción `requires`). Resultado 3097 frente al control C++ de Clang 23.1.0, de O0 a O3, con
las 11 llamadas C++ de NSC iguales a las de Clang.

Rechazos con su contraparte: `small(2.5)` (`constraints not satisfied`, con la nota de Clang) y un argumento
`string`, sin tipo C++ (`an argument has no C++ type that a template can deduce`).

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t13-deduccion-3aqlhs1c` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t13-deduccion-sf4s2yp1` |

Los fixtures de los cambios anteriores siguen pasando (`A:/temp/nsc-t13-plantillas-ns-0_0j41zq`,
`A:/temp/nsc-t13-constexpr-q9v2owoy`), y la sonda de la auditoría pasa 11 de 12 casos
(`A:/temp/nsc-t13-sonda-voj2j_bv`); queda el struct NativeSharp como argumento (cambio 5).

Regresión: en la validación agrupada de la tanda (`TANDA-13-VALIDACION-AGRUPADA.md`).

## Límites

- La ordenación parcial de C++ entre dos plantillas deducibles con la misma firma no se aplica: NativeSharp
  informa la ambigüedad.
- Un argumento sin tipo C++ (cadena, delegado, clase NativeSharp) no se deduce.
- Una llamada que nombra la plantilla a través de un método (`obj.f(x)`) o una plantilla miembro no se
  deduce.
- Un programa con llamadas a plantillas de función se analiza dos veces.
