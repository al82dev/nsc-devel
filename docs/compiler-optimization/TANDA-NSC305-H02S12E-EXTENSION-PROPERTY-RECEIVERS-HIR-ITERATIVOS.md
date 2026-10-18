# TANDA NSC305 H02S12E - receptores de propiedades de extensión HIR iterativos

## Alcance

Esta tanda elimina la arista recursiva AST a HIR usada por los getters de propiedades de
extensión. El alcance se limita a las expresiones `IdentifierExpression` y
`MemberExpression` que Sema resolvió como una `NamespaceFunctionCallBinding` con receptor
reenviado. No cambia sintaxis, resolución, ABI, NSMETA, forma pública de HIR ni política de
diagnósticos.

## Ruta histórica auditada

La implementación histórica reconocía el binding de función de namespace dentro de
`lowerExpressionImpl`, validaba su destino y, cuando existía un receptor reenviado, llamaba
recursivamente a `lowerExpression(binding.receiverSyntax, ...)`. Después aplicaba, en este
orden:

1. selección del primer parámetro enlazado;
2. elección de contexto value/address según `ParameterPassKind`;
3. conversión implícita solamente para receptores por valor;
4. envoltura de préstamo mediante `wrapCallArgumentBorrow`;
5. construcción del `StaticCall` con ordinal, nombre mangled y metadatos del argumento.

En una cadena profunda de propiedades de extensión, esa llamada recursiva consumía una
entrada de la pila del proceso por cada receptor.

## Diseño implementado

`extensionPropertyReceiverAddress` reconoce únicamente bindings completos que pueden
participar en la ruta iterativa y obtiene el contrato address/value del parámetro seleccionado.
Un nuevo tipo de frame, `ExtensionPropertyGetter`, guarda el binding y el contexto del receptor.
El worklist desciende hasta el receptor más interno sin recursión del proceso y reconstruye cada
getter al desenrollar los frames.

`lowerExtensionPropertyGetterWithReceiver` conserva en una sola frontera la construcción
histórica del HIR. Acepta el receptor ya bajado por el worklist; su fallback recursivo se conserva
solamente para entradas que no fueron clasificadas por la máquina iterativa, con lo cual no se
altera el comportamiento de rutas externas o diagnósticas existentes.

## Invariantes preservadas

- El destino debe permanecer `Resolved` y su ordinal debe existir en el modelo de funciones de
  namespace.
- Un receptor reenviado requiere sintaxis, parámetro seleccionado y parámetro de destino válidos.
- Se conservan literalmente los diagnósticos históricos de destino, contrato y parámetro.
- El contexto address se deriva exclusivamente de `ParameterPassKind`; no se infiere por tipo.
- La primera conversión seleccionada se aplica sólo al receptor por valor y sólo cuando no es
  identidad.
- El préstamo usa el mismo ordinal de callable y el mismo ordinal de parámetro.
- El `StaticCall` conserva tipo de resultado, ordinal de función, nombre mangled,
  `argumentParameterOrdinals` y `argumentTypeKeys`.
- No se inventa una validación noheap nueva: esta ruta mantiene exactamente la política previa.
- No se cambian `argumentCount`, `referenceResult`, cleanup ni el orden de evaluación.

## Prueba focal registrada

El foco `hir-extension-property-getter-depth` construye primero un proyecto mínimo real para
obtener del compilador el getter de extensión, su ordinal, tipo de receptor y nombre mangled.
Después crea una cadena sintética de 32,768 receptores enlazados y exige que:

- la bajada termine sin recursión de la pila del proceso;
- cada nodo sea un `StaticCall` con el destino real;
- todos los receptores aparezcan en el mismo orden;
- los metadatos de parámetro y tipo se preserven en cada nivel;
- la hoja siga siendo el `ThisValue` original;
- los campos que históricamente permanecían en su valor por omisión sigan sin alterarse.

La prueba utiliza `tests/unit/ProjectOopLoweringTests.cpp`, que ya pertenece en el CMake raíz al
target `nsc_project_oop_lowering_tests` y al caso CTest `nsc.project-oop-lowering`. El selector
focal y la ejecución predeterminada llaman al mismo caso, por lo que no queda una fuente, fixture
o ruta de ejecución huérfana. Esta tanda no agrega targets ni fuentes nuevas al Builder o a CMake.

## Gates estáticos ejecutados

- Auditoría de la ruta histórica y de todos sus call sites.
- Revisión del diff limitado a encabezado HIR, lowering de llamadas, máquina iterativa y prueba
  unitaria existente.
- Conteo conservador de funciones nuevas: máximo 38 líneas ejecutables aproximadas; todas por
  debajo del límite de 60.
- `git diff --check` sin errores; sólo se observaron advertencias informativas de normalización
  LF/CRLF del checkout de Windows.
- Confirmación de que no se requiere alta nueva en CMake o NSC Builder.

## Validación diferida

Por contrato de esta tanda no se compiló ni se ejecutó ninguna prueba. El foco quedó registrado
para la corrida consolidada solicitada por el propietario del proyecto. Por lo tanto, este cierre
afirma paridad de diseño y revisión estática, no evidencia dinámica.
