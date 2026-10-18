# NSC 3.0.5 — Auditoría y cierre de infraestructura de pruebas

Fecha: 2026-10-17

Rama: `development`

Punto de partida: `261f2125ca8704cc8e8aad6bea981494eafc890d`

## Alcance

Esta auditoría cubre la cadena que convierte una fuente o caso de prueba existente en una unidad realmente construible y ejecutable por NSC Builder 3:

1. fuentes del producto declaradas por el CMake raíz;
2. fuentes compiladas por `nsc_component_tests`;
3. fuentes unitarias bajo `tests/unit`;
4. módulos del registro consolidado;
5. declaración, definición y llamada de cada `register*Module` activo;
6. catálogo, asignación y manifiesto canónicos;
7. preparación de productores desde NSC Builder 3.

No se compiló NSC, no se construyó la suite y no se ejecutó ninguna prueba. La validación de esta tanda es exclusivamente estática porque la compilación y la ejecución se harán desde la otra máquina.

## Defecto reproducido

La suite configuraba y compilaba cientos de objetos, pero fallaba al enlazar con símbolos sin resolver de `ProjectMethodBodyModel`. El CMake raíz ya había acompañado la extracción del monolito con once unidades bajo `src/Frontend/Bodies`, mientras que `tests/compiler-suite/CMakeLists.txt` continuaba compilando solamente la fachada `ProjectMethodBodyModel.cpp`.

La suite también omitía `src/Parser/ParserContext.cpp`, extraído durante R01A. El shim autorizado de Parser incluye `Parser.cpp`, pero no incluye ni reemplaza `ParserContext.cpp`.

Por tanto, el fallo no era un defecto aislado de un símbolo: era una divergencia entre el grafo de fuentes del producto y el grafo que la suite recompila para probarlo.

## Inventario comprobado

- 428 fuentes `tests/unit/*.cpp` existen y las 428 están registradas en el CMake raíz.
- 227 archivos Python de NSC Builder 3 y del controlador se analizaron sintácticamente sin error.
- `CanonicalCatalog.load()` validó y unió 5,991 casos canónicos.
- El catálogo verifica unicidad y correspondencia entre diseño, asignación y manifiesto.
- La página unitaria importa `CanonicalCatalog`, expande únicamente casos ejecutables y delega la preparación a los productores derivados del campo canónico `executor`.
- La preparación de Builder configura `tests/compiler-suite` y construye el target exacto de cada productor; por ello, un gate ejecutado durante la configuración de esa suite cubre tanto la GUI unitaria como las demás superficies que reutilizan la misma preparación.
- Las fuentes HIR extraídas ya estaban sincronizadas entre el producto y la suite.
- Las once fuentes `Frontend/Bodies` y `ParserContext.cpp` eran las omisiones reales del frontend comprometido.

## Archivos históricos no ejecutables

Se encontraron catorce archivos con una función `register*Module` que no forma parte del registro activo. No deben conectarse mecánicamente:

- doce describen contratos pendientes mediante una ayuda o una disposición retirada;
- los dos módulos Linker restantes conservan descripciones de fixtures independientes pendientes y no agregan casos al registro ordinario;
- los dos módulos Host.nsc usan el antiguo estado `InfrastructurePending`, ausente del enum vigente;
- varios de los demás llaman una sobrecarga `addInfrastructureContract` que ya no existe en su soporte actual.

Estos archivos son legado de diseño de la Fase 1, no pruebas ejecutables. Quedan en una lista cerrada y documentada dentro del nuevo validador. Cualquier nuevo módulo no conectado será rechazado; ampliar la lista requerirá una decisión explícita, no ocurrirá de manera silenciosa.

## Corrección

`tests/compiler-suite/CMakeLists.txt` incorpora ahora:

- `src/Parser/ParserContext.cpp`;
- `src/Frontend/Bodies/MethodBodyBuildCoordinator.cpp`;
- `src/Frontend/Bodies/BodyScopeAndFlow.cpp`;
- `src/Frontend/Bodies/ExpressionAnalyzer.cpp`;
- `src/Frontend/Bodies/CallAnalyzer.cpp`;
- `src/Frontend/Bodies/MemberAccessAnalyzer.cpp`;
- `src/Frontend/Bodies/AssignmentAnalyzer.cpp`;
- `src/Frontend/Bodies/ConstructionAnalyzer.cpp`;
- `src/Frontend/Bodies/PatternAnalyzer.cpp`;
- `src/Frontend/Bodies/OwnershipAnalyzer.cpp`;
- `src/Frontend/Bodies/EscapeAnalyzer.cpp`;
- `src/Frontend/Bodies/StatementAnalyzer.cpp`.

Se añadió `controller/integration/validate_source_registration.py`, ejecutado automáticamente por CMake durante `configure-tests`. El validador rechaza:

- una fuente de `nsc_frontend` ausente de `nsc_component_tests`, salvo que esté cubierta por un shim realmente compilado;
- una fuente unitaria física sin registro CMake o una ruta CMake unitaria sin archivo;
- diferencias entre definición, declaración y llamada de un módulo activo;
- definiciones duplicadas de módulos;
- un módulo activo que no pertenezca a ningún target CMake;
- un nuevo módulo inactivo que no esté en la lista histórica cerrada;
- rutas de fuentes explícitas inexistentes en los CMake auditados.

## Regla obligatoria para tandas siguientes

Una prueba o extracción no se considera implementada al terminar su código. En el mismo commit deben quedar cerradas todas las capas que apliquen:

1. fuente del producto en su target raíz;
2. fuente equivalente o shim autorizado en `nsc_component_tests`;
3. fuente de prueba en su target;
4. función de registro declarada, definida y llamada;
5. caso presente en diseño, asignación, manifiesto y catálogo cuando sea un caso canónico;
6. productor reconocible por Builder;
7. guard estático actualizado únicamente si aparece una nueva categoría legítima, nunca mediante una exclusión genérica.

El objetivo del gate es detectar la omisión en `configure-tests`, antes de gastar tiempo compilando cientos de objetos y antes de llegar al enlazador.

## Evidencia estática de esta tanda

- análisis sintáctico Python: 227 archivos, sin error;
- carga canónica: 5,991 casos;
- fuentes unitarias: 428 físicas / 428 registradas;
- cobertura de `nsc_frontend` por la suite: completa después de la corrección;
- validador de registro y fuentes: salida vacía, código 0;
- compilación y pruebas: no ejecutadas por instrucción expresa.
