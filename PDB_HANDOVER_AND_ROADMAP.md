# Handover y roadmap PDB de NativeSharp / NativeSharp PDB handover and roadmap

**Fecha de corte / Snapshot date:** 2026-09-09  
**Repositorio / Repository:** \`A:\NSC-DEVEL-BRANCHES\nsc-support-pdb\`  
**Rama / Branch:** \`pdb\`  
**HEAD verificado / Verified HEAD:** \`100ce3ea9289cf40496986e2125df11e5afeb007\`  
**Build instalado / Installed build:** \`b86d060dc8b87183\`  
**Plataforma / Platform:** \`x86_64-pc-windows-msvc\`

## 1. Propósito / Purpose

**ES.** Este documento es el punto de entrada único para continuar el soporte de
depuración PDB de NativeSharp. Consolida el contrato, el trabajo ya integrado, la
evidencia ejecutada, las fronteras entre repositorios y el orden recomendado de
las entregas pendientes.

**EN.** This document is the single entry point for continuing NativeSharp PDB
debug support. It consolidates the contract, integrated work, executed evidence,
repository boundaries, and recommended order for the remaining deliveries.

La meta no es crear un formato privado. NativeSharp produce CodeView estándar en
COFF y PDB/MSF estándar, consumible por Visual Studio, WinDbg, DbgHelp,
\`llvm-pdbutil\` y el debugger de NativeSharp.

The goal is not to create a private format. NativeSharp produces standard
CodeView in COFF and standard PDB/MSF artifacts consumable by Visual Studio,
WinDbg, DbgHelp, \`llvm-pdbutil\`, and the NativeSharp debugger.

## 2. Reglas operativas / Operating rules

1. **El debugger es de solo lectura / The debugger is read-only.**
   \`A:\native-sharp-debugger\` puede inspeccionarse para conocer su contrato y
   necesidades, pero no debe modificarse desde este carril.
2. **El trabajo PDB del compilador vive aquí / Compiler PDB work lives here.**
   Usar \`A:\NSC-DEVEL-BRANCHES\nsc-support-pdb\`, rama \`pdb\`.
3. **Estado de ramas al corte / Branch state at the snapshot.**
   \`pdb\` y \`development\` apuntan al mismo commit \`100ce3ea\`; no hay commits
   pendientes de cherry-pick entre ambas.
4. **La fuente en disco es autoritativa / On-disk source is authoritative.**
   La compilación rápida debe construir e instalar el contenido actual del disco,
   no reconstruir una revisión tomada de Git.
5. **Pruebas proporcionales / Proportional testing.**
   Durante iteración se ejecutan gates dirigidos. El baseline completo se ejecuta
   una sola vez al cierre de un lote sustancial, no después de microcambios.
6. **Historia local obligatoria / Mandatory local history.**
   Cada entrega crea un MD bilingüe con archivos, funciones, implementación,
   razones, pruebas, límites y mensaje de commit copiable. Los commits locales
   llevan título y descripción en español e inglés.

## 3. Autoridades documentales / Documentary authorities

La autoridad normativa actual es:

- \`spec/NativeSharp_Debug_Artifact_Contract_1_0.md\`

El historial de implementación está en:

- \`mkdown/167_1_0_1_contrato_pdb_codeview.md\`;
- \`mkdown/168_1_0_1_primera_ruta_pdb_codeview.md\`;
- \`mkdown/169_1_0_1_parametros_locales_tipos_pdb.md\`;
- \`mkdown/170_1_0_1_miembros_struct_this_pdb.md\`;
- \`mkdown/175_1_0_1_funciones_sintetizadas_pdb.md\`;
- \`mkdown/176_1_0_1_identidad_fuente_pdb.md\`.

Los planes generales que todavía contienen criterios PDB útiles son:

- \`mkdown/121_next_steps_for_the_1_0_glory.md\`, sección 6.3;
- \`mkdown/123_plan_implementacion_0_8_alpha.md\`, Gate 5 y Gate 11.

Las necesidades del consumidor están documentadas, en modo de solo lectura, en:

- \`A:\native-sharp-debugger\docs\DEVELOPMENT_PLAN.md\`, tareas
  \`NSD-17\` a \`NSD-27\`;
- \`A:\native-sharp-debugger\docs\ROADMAP.md\`;
- \`A:\native-sharp-debugger\docs\NSC_DEBUG_RELEASE_AUDIT.md\`.

**Advertencia / Warning:** el roadmap y la auditoría del debugger todavía dicen
que NSC no emite DI/CodeView. Esa afirmación quedó obsoleta después de los commits
listados abajo. Debe corregirla el responsable del debugger; este carril no debe
editar ese repositorio.

## 4. Decisiones ya cerradas / Decisions already closed

### 4.1 Formato y artefactos / Format and artifacts

- CodeView en \`.obj\` y PDB/MSF en el producto enlazado.
- No se requiere \`.nsdbg\` ni otro sidecar privado.
- El PE contiene RSDS y el nombre registrado del PDB es basename-only.
- La identidad real PE↔PDB se valida mediante GUID/age, no sólo por nombre.
- Los artefactos actuales se producen por la ruta \`lld-link\`.

### 4.2 Perfiles / Profiles

- \`-g0\`: sin información CodeView y sin solicitud de PDB.
- \`-gline-tables-only\`: archivos, procedimientos y tablas de líneas, sin
  variables ni rangos de ubicación.
- \`-g\`: perfil NativeSharp completo soportado.
- \`-O0\` a \`-O3\` son independientes de la selección de debug.
- Si hay varias opciones \`-g*\`, gana la última.

### 4.3 Clasificación de código generado / Generated-code classification

- Código de usuario: procedimiento normal.
- Rutina sintetizada con comportamiento propio: \`DIFlagArtificial\`, no thunk,
  serializada como \`S_GPROC32\`.
- Frontera de adaptación ABI: \`DIFlagArtificial | DIFlagThunk\`, serializada
  como \`S_THUNK32\`.
- Prólogos, epílogos e instrucciones puramente sintéticas no deben convertirse
  sistemáticamente en pasos de fuente.

## 5. Entregas integradas / Integrated deliveries

| Commit | Entrega / Delivery | Estado / Status |
|---|---|---|
| \`762edf53\` | Contrato PDB/CodeView / PDB/CodeView contract | Integrado |
| \`58e84599\` | Primera ruta ejecutable: CU, archivo, función, línea, COFF, RSDS y PDB / First executable path | Integrado |
| \`f798fb82\` | Parámetros, locales, tipos y rangos O0/O2 / Parameters, locals, types, and O0/O2 ranges | Integrado |
| \`5de6158f\` | Miembros de structs, \`this\`, scopes léxicos y thunks ABI / Struct members, \`this\`, lexical scopes, and ABI thunks | Integrado |
| \`100ce3ea\` | Cctors, destrucción, filtros tipados y destrucción de closures / Cctors, destruction, typed filters, and closure destruction | Integrado |

Entre los últimos slices PDB se integraron además correcciones de interfaces,
especializaciones y metadata precompilada (\`2d6cbfef\`, \`1257ce07\`,
\`e84ca319\`, \`809e77be\`). No forman parte del formato PDB, pero son el estado
semántico sobre el que se certificó el lote actual.

## 6. Superficie disponible hoy / Surface available today

| Capacidad / Capability | Estado | Evidencia principal / Main evidence |
|---|---|---|
| Opciones \`-g0\`, \`-gline-tables-only\`, \`-g\` | Cerrada | Driver + gate |
| Compile unit, archivo, procedimiento, linkage y líneas | Cerrada para la ruta actual | \`llvm-pdbutil\`, O0/O2 |
| COFF \`.debug$S\` | Cerrada | \`llvm-readobj --sections\` |
| PDB + RSDS basename-only | Cerrada con \`lld-link\` | \`llvm-readobj --coff-debug-directory\` |
| Parámetros y locales | Cerrada para el fixture actual | \`S_LOCAL\`, \`S_DEFRANGE\`, O0/O2 |
| Valores optimizados | Parcial útil | registros/constantes en O2 |
| Tipos primitivos y struct nominal | Cerrada para el fixture actual | \`LF_STRUCTURE\`, firma y tamaño |
| Miembros de struct y offsets | Cerrada para el fixture actual | \`LF_FIELDLIST\`, \`LF_MEMBER\` |
| Receptor \`this\` | Cerrada para el fixture actual | local sobre el hogar ABI |
| Scopes léxicos | Cerrada para el fixture actual | \`S_BLOCK32\` |
| Callback guard y export C | Cerrada | \`S_THUNK32\` |
| Inicializador estático | Cerrada | \`S_GPROC32 ...cctor [generated]\` |
| Entrada de destrucción | Cerrada | \`S_GPROC32 ...destruction-entry [generated]\` |
| Filtro de catch tipado | Cerrada | \`S_GPROC32 ...catch-filter [generated]\` |
| Destructor de contexto de closure | Cerrada | \`S_GPROC32 ...closure context destroy\` |
| Ejecución del fixture | Cerrada | salida 42 en O0/O2 |
| Identidad de fuente, SHA-256 y remapeo | Cerrada en el productor | gate multiarchivo y reproducibilidad COFF |
| PDB nativo de NSLink | **Abierta, fuera de este repo** | \`NSD-20\` / \`NSL-30\` |
| Aceptación interactiva en debugger | Abierta | requiere consumidor |

## 7. Código y gates que no deben romperse / Code and gates that must not regress

### Compiler

- \`src/LLVM/ProjectOopLlvmLowerer.cpp\`
  - creación y ciclo de vida de \`DIBuilder\`;
  - \`debugType\` y construcción de firmas;
  - scopes, variables y ubicaciones;
  - \`createSynthesizedDebugSubprogram\`;
  - \`createGeneratedDebugSubprogram\`;
  - \`declareTypeInitializers\` / \`defineTypeInitializers\`;
  - \`declareDestructionEntries\` / \`defineDestructionEntries\`;
  - \`defineTypedCatchFilters\`;
  - \`closureContextDestroy\`.
- \`src/Driver/Driver.cpp\`: parseo y propagación del perfil.
- \`src/Linker/LldLinker.cpp\`: \`/debug:full\`, \`/pdb:\` y
  \`/pdbaltpath:%_PDB%\`.
- \`src/Linker/WindowsExecutableBuilder.cpp\`: derivación y propagación de la
  ruta del PDB.

### Fixture and gate

- \`tests/integration/debug_pdb/DebugPdb.nsproj\`
- \`tests/integration/debug_pdb/Main.ns\`
- \`cmake/VerifyNativeSharpDebugArtifacts.cmake\`
- test CTest: \`nsc.debug.codeview-pdb\`

El gate:

1. compila un objeto \`-gline-tables-only\`;
2. comprueba \`.debug$S\`;
3. enlaza ejecutables y PDB O0/O2;
4. ejecuta ambos programas y exige código 42;
5. valida RSDS;
6. inspecciona módulos, archivos, tipos, símbolos y líneas;
7. distingue \`S_GPROC32\` de \`S_THUNK32\`.

**Detalle importante / Important detail:** el script selecciona explícitamente
\`lld-link\`. Ejecutarlo desde una configuración general llamada NSLink no
demuestra que NSLink escriba PDB. La paridad PDB de NSLink sigue abierta.

## 8. Estado del contrato / Contract status

| Regla | Estado | Qué falta / Remaining work |
|---|---|---|
| \`NSC-DEBUG-001\` perfiles independientes | Cerrada | Mantener regresión CLI |
| \`NSC-DEBUG-002\` artefactos estándar | Cerrada con lld-link | Paridad NSLink |
| \`NSC-DEBUG-003\` perfil mínimo | Automatizado; aceptación humana abierta | Stepping real en debugger/VS/WinDbg |
| \`NSC-DEBUG-004\` perfil completo | Parcial avanzado | familias de tipos, capturas, interfaces y funclets |
| \`NSC-DEBUG-005\` identidad/reproducibilidad | Cerrada en el productor | aceptación de fuente incorrecta/remapeada en consumidores |
| \`NSC-DEBUG-006\` evidencia ejecutable | Cerrada para fixture lld-link | ampliar matriz y aceptación humana |

## 9. Roadmap de continuación / Continuation roadmap

El orden siguiente prioriza el desbloqueo del debugger y evita mezclar trabajo de
otros repositorios con el compilador.

### PDB-P0 — Aceptación real del slice existente / Real acceptance of the existing slice

**Objetivo / Goal:** entregar al debugger un fixture actual y comprobar que puede
consumirlo sin suposiciones privadas.

- usar los \`debug_o0.exe/.pdb\` y \`debug_o2.exe/.pdb\` producidos por el gate;
- validar GUID/age RSDS;
- comprobar dirección↔función↔archivo/línea;
- colocar breakpoint en \`Main.ns\`;
- inspeccionar \`left\`, \`right\`, \`total\`, \`pair\`, \`this\` y \`answer\`;
- recorrer stack a través de callback, excepción y función sintetizada;
- registrar cualquier dato estándar que el consumidor no pueda interpretar.

**Frontera / Boundary:** este repo corrige el productor. Los cambios de consumo
pertenecen a \`A:\native-sharp-debugger\` y requieren autorización separada.

**Cierre / Exit:** el fallo queda clasificado como productor, linker o consumidor,
con repro mínimo. No se acepta “PDB no funciona” sin identificar la capa.

### PDB-P1 — Identidad de fuente, checksums y remapping

**Estado del productor / Producer status:** implementado después del corte de este handover; ver
\`mkdown/176_1_0_1_identidad_fuente_pdb.md\`. La aceptación interactiva del consumidor permanece
en PDB-P0/PDB-P7.

**Objetivo / Goal:** cerrar \`NSC-DEBUG-005\` sin publicar rutas privadas.

- emitir checksums de contenido soportados por LLVM/CodeView;
- normalizar separadores, casing contractual y rutas Unicode;
- definir una opción documentada de prefix/path remapping;
- conservar la identidad correcta en proyectos multiarchivo y referencias;
- agregar negativos para fuente movida, modificada y con mismo basename;
- comparar significado reproducible, no bytes inestables del linker.

**Cierre / Exit:** el debugger puede rechazar una fuente incorrecta y resolver una
fuente remapeada sin conocer \`A:\` ni el checkout original.

**Commit sugerido / Suggested commit:**  
\`Agrega identidad reproducible de fuentes PDB / Add reproducible PDB source identity\`

### PDB-P2 — Interfaces, dispatch y thunks restantes

**Objetivo / Goal:** describir el camino real de una llamada de interfaz.

- inventariar tablas, stubs y thunks creados por el lowering;
- clasificar adaptación ABI como thunk y comportamiento propio como procedimiento
  artificial;
- conservar display name, linkage y origen estable;
- probar métodos virtuales/de interfaz, handlers estáticos/de instancia y structs
  ABI por valor/indirectos;
- auditar \`.pdata\`/\`.xdata\` y stack walking.

**Cierre / Exit:** un stack de interfaz muestra el método lógico y las fronteras
generadas sin confundirlas con fuente del usuario.

**Commit sugerido / Suggested commit:**  
\`Describe dispatch de interfaces en PDB / Describe interface dispatch in PDB\`

### PDB-P3 — Funclets SEH y excepciones completas

**Objetivo / Goal:** hacer depurables try/catch/finally, cleanup y throw.

- identificar filtros, catchpads, cleanuppads y continuaciones producidas;
- asignar scope y ubicación sin inventar líneas de usuario;
- verificar unwind físico y transición de frames;
- cubrir throw, catch tipado, catch general, finally, destructor durante unwind y
  callback C;
- probar funciones que pasan por \`mem2reg\` aun en O0 por requerimientos SEH.

**Cierre / Exit:** breakpoint, stack y stepping atraviesan una excepción con línea
y frame correctos en O0/O2.

**Commit sugerido / Suggested commit:**  
\`Completa la representación PDB de SEH / Complete PDB representation for SEH\`

### PDB-P4 — Tipos NativeSharp y closures completos

**Objetivo / Goal:** pasar del fixture mínimo a valores representativos del
lenguaje nativo.

- enums y underlying type;
- clases, bases, campos, punteros y referencias;
- arrays, spans y strings;
- delegates y contexto de closure;
- variables capturadas con sus ubicaciones;
- genéricos materializados y nombres estables;
- handles nominales;
- unions, vectores y matrices cuando sus modelos sean autoritativos;
- enumeradores y demás superficies Core necesarias para inspección.

No se deben inventar semánticas C# que NativeSharp no tiene. El PDB describe el
modelo NativeSharp y su layout físico real.

**Cierre / Exit:** \`NSD-25\` y \`NSD-26\` pueden presentar scopes, parámetros,
locales y tipos sin offsets o lifetimes falsos.

**Commit sugerido / Suggested commit:**  
\`Amplía los tipos NativeSharp en PDB / Expand NativeSharp types in PDB\`

### PDB-P5 — Fidelidad bajo optimización

**Objetivo / Goal:** diagnóstico de producción honesto.

- valores en registro, stack, constante, fragmentados u optimizados fuera;
- rangos discontinuos;
- inlining y frames inline, si el pipeline lo soporta;
- tail calls y funciones fusionadas;
- ramas, loops y múltiples instrucciones por línea;
- columnas y discriminadores donde aporten precisión real.

Nunca crear una ubicación obsoleta sólo para mostrar un valor.

**Cierre / Exit:** O2 muestra lo disponible y marca honestamente lo eliminado; el
stepping no promete equivalencia artificial con O0.

**Commit sugerido / Suggested commit:**  
\`Endurece el PDB para código optimizado / Harden PDB for optimized code\`

### PDB-P6 — Paridad de linker

**Objetivo / Goal:** cerrar la diferencia entre lld-link y NSLink.

El trabajo principal pertenece a \`A:\nslink-devel\` (\`NSD-20\` / \`NSL-30\`):

- fusionar contribuciones CodeView de objetos;
- construir streams MSF/PDB, DBI, TPI/IPI, publics y módulos;
- aplicar relocations;
- escribir GUID/age y RSDS coincidente;
- soportar opciones y diagnósticos;
- comparar semántica con lld-link.

Este repo debe aportar fixtures y comprobar que sus objetos contienen todo lo
necesario, pero no debe esconder la ausencia de un escritor PDB de NSLink.

**Cierre / Exit:** el mismo fixture presenta comportamiento observable equivalente
en ambos linkers y \`llvm-pdbutil\` acepta ambos PDB.

### PDB-P7 — Certificación final

- Visual Studio, WinDbg y NativeSharp Debugger;
- O0 y O2;
- lld-link y NSLink;
- proyecto multiarchivo y rutas Unicode;
- callbacks, interfaces, excepción, destrucción y closures;
- PDB presente, ausente, corrupto y de otro build;
- source correcto, remapeado y checksum incorrecto;
- stack, breakpoints, stepping, locales y tipos;
- reproducibilidad medida;
- baseline G00 una sola vez al cierre.

**Cierre final / Final exit:** colocar un breakpoint en NativeSharp, inspeccionar
una local, entrar a una llamada, atravesar una excepción, recorrer una llamada
virtual/de interfaz y obtener fuente/línea correctas después de optimizar.

## 10. Relación con las tareas del debugger / Debugger task mapping

| Tarea debugger | Estado visto desde NSC / State from NSC |
|---|---|
| \`NSD-17\` contrato de artefactos | Parcialmente satisfecho por el spec; faltan versión compartida, checksums, remapping y negativos |
| \`NSD-18\` NSASM | No pertenece a este repo |
| \`NSD-19\` metadata NSC | Ya no está bloqueado; slice mínimo y varias extensiones están integrados |
| \`NSD-20\` NSLink PDB/RSDS | Sigue bloqueado fuera de este repo |
| \`NSD-21\` confianza de símbolos | Desbloqueado para comenzar con PDB lld-link; checksums/remapping pendientes |
| \`NSD-22\` breakpoints fuente | Desbloqueado para fixture lld-link |
| \`NSD-23\` stepping fuente | Parcialmente desbloqueado; SEH/interfaces/O2 requieren más cobertura |
| \`NSD-25\` scopes/locales | Desbloqueado para la superficie del fixture |
| \`NSD-26\` tipos | Parcialmente desbloqueado; tipos completos pendientes |

## 11. Evidencia del último punto estable / Last stable-point evidence

Commit certificado: \`100ce3ea\`.

Gate dirigido:

- PDB O0/O2: \`NativeSharp O0/O2 CodeView and PDB artifacts verified\`;
- ejecución O0: 42;
- ejecución O2: 42;
- \`nsc_project_oop_lowering_tests\`: 71 checks;
- \`nsc_stage37_static_initialization_tests\`: 60 checks;
- \`nsc_stage45_11_eh_cleanup_execution_tests\`: 72 checks;
- \`nsc_stage50_11_ownership_execution_tests\`: 196 checks;
- \`nsc_stage67_e11_lambda_execution_tests\`: 4 checks.

Baseline global:

- corrida: \`A:\nsc-devel\_nscbuilder\history\20260909-155950-ff92fdc8\`;
- build instalado: \`b86d060dc8b87183\`;
- CTest lld-link: 307/307;
- fixtures lld-link: 148/148;
- triage lld-link: 18/18;
- CTest NSLink: 307/307;
- fixtures NSLink: 148/148;
- triage NSLink: 18/18;
- Gate 0: \`LANDED\`;
- duración: 1541.36 s.

El baseline de ambas configuraciones demuestra que el compilador conserva
compatibilidad general. No sustituye la certificación PDB nativa de NSLink
descrita en PDB-P6.

## 12. Secuencia de verificación / Verification sequence

Durante un slice:

1. construir únicamente \`nsc\` y los tests afectados;
2. ejecutar \`nsc.debug.codeview-pdb\`;
3. ejecutar suites semánticas relacionadas con las rutas tocadas;
4. inspeccionar el PDB con \`llvm-pdbutil\` y el PE con \`llvm-readobj\`;
5. ejecutar los productos O0/O2;
6. hacer aceptación manual sólo cuando el artefacto automatizado esté verde.

Al cierre de un lote sustancial:

\`\`\`powershell
python A:\NSC-DEVEL-BRANCHES\nsc-support-pdb\nscbuilder.py --baseline --jobs 4
\`\`\`

No correr el baseline después de cada cambio pequeño.

## 13. Riesgos y detalles conocidos / Known risks and details

- El gate usa expresiones regulares sobre la salida de \`llvm-pdbutil\`; valida
  estructura estándar, pero no sustituye una sesión real.
- Las líneas centinela 45 y 105 cambian si se edita el fixture; deben actualizarse
  deliberadamente.
- El filtro de catch tipado es compartido por tipo y usa la declaración del tipo
  como origen estable, no un catch individual.
- El destructor de contexto localiza la lambda propietaria por linkage; cualquier
  cambio de mangling debe mantener esa asociación y su prueba.
- Algunos punteros pueden mostrarse en dumps como simple type \`0x0600\`; antes de
  cambiar el productor hay que verificar cómo lo consumen Visual Studio, WinDbg y
  el debugger.
- Una función leaf puede no tener \`.pdata\`; el requisito es stack walking
  correcto, no una entrada unwind por símbolo.
- El trabajo de NSLink y NSASM no debe implementarse dentro de NSC.
- El debugger sigue siendo de solo lectura para este carril.

## 14. Primer paso recomendado / Recommended first step

Ejecutar PDB-P0 con el fixture ya certificado y entregar al responsable del
debugger:

1. EXE/PDB O0 y O2;
2. GUID/age y dump semántico;
3. lista de símbolos y variables esperadas;
4. casos de breakpoint, stack y excepción;
5. esta matriz de estado actualizada.

PDB-P1 ya quedó implementado del lado productor después del corte. Tras la aceptación PDB-P0,
corresponde clasificar cualquier brecha encontrada; si no aparece un bloqueo más urgente, el
siguiente slice del productor es PDB-P2 (interfaces, dispatch y thunks restantes).

## 15. Mensaje de commit de este handover / Commit message for this handover

### Título ES / Spanish title

\`Entrega el handover y roadmap PDB\`

### Title EN / English title

\`Add the PDB handover and roadmap\`

### Descripción ES / Spanish description

\`Consolida el estado certificado del soporte CodeView/PDB, las fronteras con el debugger y NSLink, la evidencia vigente y la secuencia priorizada para completar identidad de fuentes, interfaces, SEH, tipos, optimización y certificación.\`

### Description EN / English description

\`Consolidate the certified CodeView/PDB support state, debugger and NSLink boundaries, current evidence, and the prioritized sequence for completing source identity, interfaces, SEH, types, optimization, and certification.\`
