# NSC 3.0.5 — B03: dispatcher de expresiones

Fecha: 2026-10-17
Rama: `development`

## Objetivo

Separar la coordinación del análisis de expresiones de
`ProjectMethodBodyModel.cpp` y retirar la búsqueda cuadrática del cache usado por el recorrido
iterativo de expresiones profundas. No se modifica ninguna regla del lenguaje.

## Ejecución en dos commits

La tanda se dividió deliberadamente para no mezclar movimiento con cambio algorítmico:

1. `98f475548` mueve el dispatcher y la máquina de worklist a
   `src/Frontend/Bodies/ExpressionAnalyzer.cpp`.
2. El commit de cierre sustituye el lookup lineal del cache por un índice hash privado.

## Frontera física

El nuevo módulo contiene:

- `analyzeExpression`;
- `cachedExpressionResult`;
- `appendIterativeExpressionChildren`;
- `shouldAnalyzeExpressionIteratively`;
- `analyzeExpressionIteratively`.

`analyzeExpressionCore` permanece temporalmente en el monolito como núcleo semántico. B03 no
arrastra sus familias: calls corresponden a B04; assignments/members a B05;
construction/patterns a B06; ownership/escape a B07. Esas tandas podrán reducir el core sin
convertir este movimiento en un cambio semántico masivo.

## Corrección algorítmica

Antes, `cachedExpressionResult` ejecutaba `find_if` sobre todos los resultados ya producidos.
La worklist lo consulta al sacar un frame, al insertar hijos y al recuperar el resultado final.
Para una expresión con N frames, esa combinación permitía costo O(N²).

La identidad completa de una entrada se conserva:

- puntero estable al nodo AST;
- `allowMethodGroup`;
- `allowEvent`;
- `allowMove`;
- `allowConstructorPropertyWrite`.

Los cuatro flags se compactan en una máscara de 4 bits. La pareja `(syntax, optionMask)` indexa el
ordinal del resultado en un `unordered_map`. El vector de resultados se conserva para mantener
ownership, orden de publicación y direcciones válidas durante cada consulta. El costo esperado del
lookup pasa a O(1), sin omitir ningún contexto que pudiera cambiar la semántica.

## Contratos conservados

- mismas firmas de `ProjectMethodBodyModel`;
- mismo orden postorder izquierda-a-derecha;
- mismos resultados, ordinals y diagnostics;
- mismas excepciones para operadores lógicos, lambdas, null-conditional y referencias de namespace;
- sin cambios de sintaxis, ABI, layout, ownership, NSMETA o formatos;
- sin paralelismo sobre estado mutable compartido.

## Gate focal

Se construyó y ejecutó:

`A:\nsc-devel\build-test-infrastructure\tests\nsc_project_method_body_model_tests.exe`

Resultado: `PASS: 76 checks`.

El gate incluye `testDeepNestedValueExpression`, que genera 40 niveles alternados de llamada y
expresión binaria y exige que el binder los procese mediante la worklist alojada en heap. También
incluye cadenas lógicas largas y la comparación determinista del modelo entre j1, j2 y j12.

Se construyó además:

`cmake --build A:\nsc-devel\build-clang --target nsc -j 16`

Resultado: `nsc.exe` construido y contrato static host-runtime passed.

No se ejecutaron baseline, deploy, instalación, batería completa ni consumidores externos. La ruta
histórica de CTest en C: sólo fue mostrada por CMake; no se ejecutó ni modificó.

## Resultado

B03 elimina un recorrido lineal repetido de la ruta de expresiones profundas y deja el dispatcher
en una unidad coherente. No se atribuye un porcentaje de aceleración sin una medición aislada; la
mejora demostrable aquí es de complejidad algorítmica y equivalencia focal.
