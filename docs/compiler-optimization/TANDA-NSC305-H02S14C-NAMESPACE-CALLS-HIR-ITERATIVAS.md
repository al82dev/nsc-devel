# NSC 3.0.5 - H02S14C: namespace calls HIR iterativas

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa de receptor reenviado y argumentos
  explicitos durante el lowering HIR de llamadas a funciones de namespace.
- Base de trabajo aislada: `e70dd49c2805c918182665d16d6abe6000970aa7`, equivalente a
  H02S14A+H02S14B ya integrados en `development`.
- Archivos autorizados: modelo HIR privado, lowering de calls, maquina comun de expresiones,
  prueba focal y este expediente.
- Frontera protegida: eventos H02S14D, delegate invoke H02S14E, core H02S14F-K, llamadas
  ordinarias H02S14L, ABI, NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: selector `hir-namespace-call-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

`ProjectOopHirModel::lowerCallExpression` resolvia correctamente el binding namespace, pero
descendia mediante `lowerExpression` tanto en el receiver sintetico de extensiones como en cada
argumento explicito. Una cadena `Identity(Identity(...))` consumia una trama nativa por nivel.
La rama tambien mezclaba descenso, conversion, empaquetado, borrow, defaults, cleanup y retorno
por referencia, impidiendo que el worklist comun controlara el orden completo.

## Diseno implementado

1. `IterativeExpressionKind::NamespaceCall` clasifica llamadas namespace que tienen receptor
   reenviado o argumentos explicitos.
2. `NamespaceCallFrameState` congela binding, target, nodo `StaticCall`, ordinal callable,
   estado receiver/arguments/ready, pack expandido y ownership temporal.
3. La inicializacion valida una vez el binding y resuelve una sola vez el
   `NamespaceFunctionSymbol` que consumira el frame.
4. El scheduler visita receptor primero y despues argumentos explicitos en orden fuente. Los
   argumentos explicitos siempre se bajan en contexto de valor; un receiver ref se baja en
   contexto de direccion.
5. La reanudacion aplica conversion antes de pack/borrow, agrupa elementos pack en un unico nodo
   estable y actualiza paralelamente ordinals y type keys.
6. `finishNamespaceCall` no desciende en expresiones. Agrega defaults, sintetiza pack vacio,
   publica `argumentCount`/`referenceResult`, envuelve cleanup y materializa `PointerLoad` una vez
   cuando un retorno ref se consume como valor.
7. La ruta historica de `lowerCallExpression` conserva solamente calls namespace sin hijos. Si
   una call con receptor o argumentos escapara del scheduler, falla internamente en lugar de
   reintroducir recursion.

Complejidad para profundidad D: O(D) tiempo, O(D) frames explicitos y O(1) pila nativa adicional
por arista.

## Contratos preservados

- destino, ordinal namespace, nombre mangled, tipo resultante y ordinal callable;
- orden receiver -> argumentos explicitos -> defaults -> pack vacio;
- receiver como primer hijo y conversion exclusiva del receiver por valor;
- argumentos explicitos siempre bajados como valores, incluso para parametros ref;
- conversion antes de pack o borrow;
- un solo `ParameterPack`, orden estable de elementos y ausencia de borrow por elemento;
- defaults sin conversion ni borrow;
- `argumentParameterOrdinals` y `argumentTypeKeys` paralelos a hijos fisicos;
- borrow de temporal owned con un solo `FullExpressionCleanup`;
- retorno ref como call direccionable o un solo `PointerLoad` en contexto de valor;
- textos exactos de diagnostico para target, addressability, receiver, ordinals, defaults y packs.

## Prueba focal

El selector `hir-namespace-call-depth` cubre 22 checks:

- proyecto minimo real con `ProjectNamespaceFunctionModel` y binding real por nodo;
- 32,768 calls namespace anidadas en el primer argumento hasta un literal;
- recorrido y destruccion HIR iterativos;
- conversion + owned borrow + cleanup unico;
- pack de dos elementos y pack vacio;
- default argument;
- retorno ref en contexto de valor y direccion;
- receiver reenviado value y ref;
- los once diagnosticos contractuales exactos de S14C.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests -j 16`.
- Resultado: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-namespace-call-depth`.
- Resultado: `PASS: 22 checks`, exit 0.
- Log de compilacion:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14c-build-final.log`.
- Log focal:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14c-focus-final.log`.
- Auditoria estatica:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14c-static-audit.log`.

## Auditoria estatica de cierre

- El bloque namespace residual de `lowerCallExpression` contiene cero llamadas a
  `lowerExpression` y una sola llamada al finalizador.
- El frame guarda un puntero const al target resuelto durante inicializacion; no vuelve a buscarlo
  al aceptar hijos ni al finalizar.
- `acceptNamespaceCallChild` tiene un unico sitio de llamada en la maquina.
- `finishNamespaceCall` tiene un unico sitio de llamada desde la ruta iterativa y no llama a
  `lowerExpression`, `lowerCallExpression` ni a si mismo.
- No se modificaron eventos, delegate invoke, core calls, ordinary calls, ABI, NSMETA, parser,
  MIR, LLVM, driver ni tracking global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
