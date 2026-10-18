# NSC 3.0.5 — L06H: análisis LLVM inmutable compartido

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\NSC-devel-paralelo\development-agente-5-l06`,
  `codex/development-agente-5-l06`, `bde74eee72b20bb2ea7364e16c45714f08d6bf4f`.
- **Objetivo exacto:** eliminar recomputaciones cold del cierre MIR y de las máscaras de campos
  entre planificación, caché, coordinador y módulos LLVM, y atribuir todo el preámbulo de la fase.
- **Naturaleza:** optimización interna no funcional y telemetría causal de L06.
- **Evidencia real de entrada:** build GUI Release O0, build hash `867e4a607a5a32a1`, 9 módulos,
  0 hits/9 misses, `llvm-object-generation=34.291 s`, frontend `23.308 s`, enlace `8.669 s`
  y total `72.3 s`. Los logs actuales de `A:\temp` fueron sobrescritos por otra ejecución y no
  se atribuyen a esa corrida.
- **Causa estática:** `projectOopReachableDefinitionMask` se ejecuta al formar la clave del plan,
  construir la partición, preparar el generador, calcular el cierre del coordinador y construir
  cada lowerer. `projectOopStaticFieldDeclarationMask` se ejecuta una vez para telemetría y otra
  dentro del lowerer por cada módulo. Cada repetición reconstruye índices y recorre MIR.
- **Corrección autorizada:** construir una sola vista inmutable de alcanzabilidad por invocación,
  compartirla con clave/partición/coordinador/lowerers y reutilizar por módulo una sola máscara de
  campos para telemetría y lowering. La identidad MIR se construye una sola vez cuando la caché
  pre-LLVM es elegible, como actualmente, y se comparte para todas las claves de unidad.
- **Telemetría autorizada:** esquema 4 de `A:\temp\nsc-llvm-code-unit-timing.log`, autenticado por
  identidad estable de codegen, huella privada del proyecto, debug y optimización; tiempos `partition-key`,
  `partition-restore`, `partition-build`, `reachability`, `coordinator-closure`,
  `identity-index-build`, `line-table-identity`, `module-key-compose`, `static-field-mask`,
  `worker-wall` y `plan-store`.
- **Archivos autorizados:**
  - `CMakeLists.txt`, sólo para registrar el nuevo módulo LLVM en los targets existentes;
  - `include/nativesharp/LLVM/ProjectOopFunctionReachability.h`;
  - `src/LLVM/ProjectOopFunctionAnalysis.cpp`;
  - `src/LLVM/ProjectOopFunctionReachability.cpp`;
  - `include/nativesharp/LLVM/ProjectOopCodePartition.h`;
  - `src/LLVM/ProjectOopCodePartition.cpp`;
  - `include/nativesharp/LLVM/ProjectOopLlvmLowerer.h`;
  - `src/LLVM/ProjectOopLlvmLowerer.cpp`;
  - `include/nativesharp/LLVM/ProjectOopObjectGenerator.h`;
  - `src/LLVM/ProjectOopObjectGenerator.cpp`;
  - `tests/unit/ProjectOopLoweringTests.cpp`;
  - este expediente.
- **Contratos congelados:** sintaxis, semántica, diagnósticos, API/ABI del lenguaje, NSMETA,
  nombres físicos, selección y orden de funciones/campos, claves y formato de caché, invalidación,
  particiones, LLVM/COFF, CLI y salida normal.
- **Prueba focal:** selector nuevo `llvm-shared-analysis` del target exacto
  `nsc_project_oop_lowering_tests`; debe comparar análisis compartido contra la ruta compatible,
  partición, máscaras y LLVM normalizado.
- **Prohibido:** HIR, parser, bindings, consumidores, juegos, linker, Builder/GUI, CTest, suite,
  minibaseline, baseline, deploy, instalación, red y cualquier escritura fuera de `A:`.
- **Criterio de terminación:** una sola construcción de alcanzabilidad por generación, cero
  reconstrucciones en partition/lowerers de esa ruta, paridad focal exacta, target focal limpio,
  telemetría causal completa y commit local aislado.

## Estado inicial

Implementación pendiente. La evidencia temporal real sólo demuestra que LLVM continúa costando
34.291 s; la ganancia de esta tanda no se declarará hasta una corrida comparable del usuario.

## Implementación

- `ProjectOopFunctionReachabilityAnalysis` construye una sola vez por emisión los índices por
  nombre y miembro, la alcanzabilidad transitiva iterativa, el índice de campos estáticos y una
  huella privada del catálogo MIR. No modifica ni publica ningún contrato del lenguaje.
- La clave persistente y la construcción de particiones reciben la misma máscara inmutable. Sus
  rutas compatibles siguen calculándola localmente cuando un consumidor antiguo no la entrega.
- El coordinador deriva su cierre de declaraciones sobre los índices compartidos, preservando la
  inclusión de inicializadores y tablas de despacho.
- Cada unidad calcula una sola máscara de campos. La misma vista alimenta telemetría y lowering;
  el lowerer la consume por `span`, sin segunda caminata ni copia por módulo.
- Los lowerers reciben también la alcanzabilidad compartida por `span`. Las rutas públicas sin
  análisis previo conservan el comportamiento anterior mediante almacenamiento propio.
- La traza temporal sube a esquema 4 y registra identidad estable de codegen, huella privada del
  proyecto y desglose del preámbulo: reachability, clave/restauración/construcción de partición,
  cierre del coordinador, construcción del índice de identidad, identidad de line tables,
  composición de claves, máscaras de campos, pared de workers y publicación del plan.

## Invariantes comprobadas por el contrafactual

El selector `llvm-shared-analysis` construye MIR real y compara la implementación histórica con
la compartida. Comprueba exactamente:

1. máscara de definiciones alcanzables;
2. cierre de declaraciones del coordinador;
3. clave persistente de partición;
4. dump y payload persistente de la partición;
5. máscaras del coordinador y de cada unidad;
6. construcción correcta de ambos módulos;
7. texto LLVM byte a byte idéntico;
8. huella privada no vacía.

## Evidencia focal

- Configuración: Release, `clang-cl`/`lld-link`/`llvm-rc` resueltos por nombre desde el `PATH`;
  build y temporales exclusivamente en `A:\NSC-devel-paralelo`.
- Target único: `nsc_project_oop_lowering_tests` — **PASS**.
- Selector único: `NSC_PROJECT_OOP_LOWERING_FOCUS=llvm-shared-analysis` — **PASS 9/9**,
  `exit 0`.
- No se ejecutaron CTest, suite, minibaseline, baseline, deploy ni instalación.

## Estado de cierre

La recomputación estática identificada queda eliminada y la equivalencia queda demostrada por la
prueba focal. La cifra de `llvm-object-generation` posterior no se inventa ni se infiere: deberá
obtenerse después con una corrida GUI comparable del consumidor real. Ese dato sirve para evaluar
la ganancia y decidir el siguiente perfil, no para reabrir la implementación ya validada aquí.
