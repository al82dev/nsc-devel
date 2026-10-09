# Host.Frontend — inventario semántico y contrato reconstructivo

## Estado y alcance

- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Estado:** `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada.
- **Universo:** 121 archivos, 74,688 líneas y 3,623,063 bytes.
- **Símbolos inventariados:** 1,392: 271 tipos, 1,101 definiciones físicas y 20 operaciones inline.
- **Naturaleza:** modelos de proyecto, resolución, genéricos, cuerpos, ownership, construcción, HIR y MIR.
- **Fuera de alcance:** modificar código o pruebas, compilar, ejecutar, medir cobertura, instalar o desplegar.

## 1. Frontera del componente

`Host.Frontend` recibe unidades parseadas, grafo de proyecto, metadata precompilada y configuración de target.
Produce modelos de nombres, miembros, herencia, interfaces, genéricos, cuerpos, construcción, destrucción,
layout, HIR y MIR. También publica dumpers deterministas y estadísticas de los recorridos. La división de este
documento sigue contratos observables; no replica la estructura de control interna.

La revisión conserva todas las decisiones vigentes, incluso textos históricos, límites conservadores y
recorridos recursivos. Registrarlos no autoriza corregirlos durante esta fase documental.

## 2. Contratos propietarios

| Contrato | Familia propietaria | Responsabilidad estable |
|---|---|---|
| `CF-FRONT-001` | integer helpers | clasificar builtins, suffixes y tipo de literal entero |
| `CF-FRONT-002` | enum underlying | resolver el underlying explícito o default de enums |
| `CF-FRONT-003` | stages | contrato, tracker y verificación de fronteras de etapa |
| `CF-FRONT-004` | `FrontendCompilation` | pipeline histórico de una unidad y sus modelos |
| `CF-FRONT-005` | `ProjectSyntaxUnit` | cargar, parsear y conservar una unidad del proyecto |
| `CF-FRONT-006` | namespaces y nombres | árbol de namespaces y escalera de nombres calificados |
| `CF-FRONT-007` | `ProjectNameLookup` | predeclarar módulos, tipos, funciones y estado |
| `CF-FRONT-008` | `ProjectNameLookup` | enlazar scopes, tipos, expresiones, aliases y visibilidad |
| `CF-FRONT-009` | name lookup dumper | vista textual determinista de nombres y bindings |
| `CF-FRONT-010` | precompiled identity | indexar y resolver identidades físicas precompiladas |
| `CF-FRONT-011` | accessibility | mapear y comparar accesibilidad de miembros |
| `CF-FRONT-012` | `ProjectMemberModel` | descubrir miembros, firmas, cuerpos y nombres físicos |
| `CF-FRONT-013` | `ProjectMemberModel` | lifetime, ownership, drop, copy y move transitivos |
| `CF-FRONT-014` | `ProjectMemberModel` | enums, promociones anónimas y validación contextual |
| `CF-FRONT-015` | member dumper | vista textual determinista del catálogo de miembros |
| `CF-FRONT-016` | inheritance | construir y validar bases, ciclos y relaciones derivadas |
| `CF-FRONT-017` | interfaces | contratos, slots e implementaciones de interfaces |
| `CF-FRONT-018` | virtual dispatch | vtables, overrides y despacho de interfaces |
| `CF-FRONT-019` | namespace functions | declarar, resolver y enlazar funciones de namespace |
| `CF-FRONT-020` | generics | descubrir templates, aliases, dependencias y visibilidad |
| `CF-FRONT-021` | generics | inferir argumentos y materializar especializaciones |
| `CF-FRONT-022` | generic constraints | normalizar, satisfacer y ordenar restricciones |
| `CF-FRONT-023` | generic dumper | vista textual de templates y especializaciones |
| `CF-FRONT-024` | conversions | clasificar conversiones nominales, estructurales y C ABI |
| `CF-FRONT-025` | integer evaluator | evaluar constantes enteras con width, signo y traps |
| `CF-FRONT-026` | flow records | snapshots, rangos, storage facts y estadísticas |
| `CF-FRONT-027` | callable records | contexto, resultado e índices por callable/syntax |
| `CF-FRONT-028` | callable waves | scheduling, reducción y merge determinista de resultados |
| `CF-FRONT-029` | `ProjectCompilation` | orquestar etapas, jobs, cancelación y modelos de proyecto |
| `CF-FRONT-030` | compilation dumper | vista textual del estado de compilación de proyecto |
| `CF-FRONT-031` | method bodies | construir callables y fusionar análisis aislados |
| `CF-FRONT-032` | method bodies | scopes, nombres, locales y definite assignment |
| `CF-FRONT-033` | method bodies | snapshots, joins, partial moves y estado de borrows |
| `CF-FRONT-034` | method bodies | statements, loops, switch, match y retornos |
| `CF-FRONT-035` | method bodies | excepciones, using, cleanups y salidas estructuradas |
| `CF-FRONT-036` | method bodies | traversal, caché, tipado y despacho de expresiones |
| `CF-FRONT-037` | method bodies | identificadores, miembros y estado de namespace |
| `CF-FRONT-038` | method bodies | calls, argumentos, overloads y candidatos sintéticos |
| `CF-FRONT-039` | method bodies | unary, binary, promociones y facts numéricos |
| `CF-FRONT-040` | method bodies | assignment, deconstruction y accessors compuestos |
| `CF-FRONT-041` | method bodies | casts, runtime types, optional y result propagation |
| `CF-FRONT-042` | method bodies | ownership, move, borrow, views y escape interprocedural |
| `CF-FRONT-043` | method bodies | lambdas, captures, delegates y events |
| `CF-FRONT-044` | method bodies | Core APIs, atomics, memoria, SIMD e intrinsics |
| `CF-FRONT-045` | method body dumper | vista textual de bindings y postestado semántico |
| `CF-FRONT-046` | escape evaluator | evaluar orígenes de referencia con worklist explícita |
| `CF-FRONT-047` | construction | sites, resolución de constructores e inicializadores |
| `CF-FRONT-048` | construction dumper | vista textual de resoluciones y planes de construcción |
| `CF-FRONT-049` | destruction | planes de destrucción, drop y dumper observable |
| `CF-FRONT-050` | object layout | layouts, variantes, fields, padding y dumper |
| `CF-FRONT-051` | comptime | cache, límites, evaluación, atributos y diagnósticos authored |
| `CF-FRONT-052` | macros | expandir invocaciones con límite y origen estable |
| `CF-FRONT-053` | metaprogramming | registrar y resolver operaciones metaprogramadas |
| `CF-FRONT-054` | opaque results | inferir y estabilizar resultados opacos |
| `CF-FRONT-055` | inlining/cost | política de inline y exploración de costo alcanzable |
| `CF-FRONT-056` | HIR | construir, paralelizar y fusionar funciones HIR |
| `CF-FRONT-057` | HIR | bajar statements, control, excepciones y cleanups |
| `CF-FRONT-058` | HIR | bajar expresiones, calls, receivers y conversiones |
| `CF-FRONT-059` | HIR | ownership, construcción, aggregates, closures y Core APIs |
| `CF-FRONT-060` | HIR dumper | vista textual determinista del HIR |
| `CF-FRONT-061` | MIR | construir contratos ABI, layouts, dispatch y símbolos físicos |
| `CF-FRONT-062` | MIR | bajar nodos, runtime, EH, atomics, SIMD y entry point |
| `CF-FRONT-063` | MIR dumper | vista textual determinista del MIR |

## 3. Contratos 001–010: entrada, etapas y nombres

- Los helpers enteros conservan tablas y transiciones exactas de suffix, signo, width y rango.
- El underlying de enum acepta únicamente la forma codificada y aplica el default vigente cuando falta.
- Cada etapa publica un conjunto de información; el tracker sólo permite avances compatibles y el verificador
  rechaza consumidores adelantados.
- `FrontendCompilation` sigue siendo una ruta histórica distinta de la compilación completa de proyecto.
- Una `ProjectSyntaxUnit` mantiene fuente, AST, identidad de proyecto/módulo y estado de parseo coherentes.
- Los namespaces preservan parent, children y símbolos; la escalera produce candidatos de más específico a
  menos específico sin duplicados.
- Name lookup separa predeclaración de binding y aplica visibilidad por proyecto, módulo, reexport y nesting.
- El índice precompilado exige identidad física inequívoca; colisiones o metadata incompleta no se inventan.

## 4. Contratos 011–019: miembros, herencia e interfaces

- La accesibilidad se compara desde el contexto consumidor y no sólo por spelling.
- Member model conserva orden de descubrimiento, firma canónica, modifiers, parameter pass kind y cuerpo.
- Ownership y custom destruction se propagan transitivamente con caches estables; copy/move se derivan de ese
  cierre y no de una marca aislada.
- Enums conservan width, signo, incremento, overflow y bits; miembros anónimos se promueven con colisiones
  explícitas.
- Herencia detecta bases inválidas y ciclos antes de publicar relaciones derivadas.
- Interfaces separan contrato, slot, implementación y dispatch; missing/ambiguous implementations diagnostican.
- Virtual dispatch mantiene orden de slots, overrides finales/abstractos y tablas por tipo.
- Namespace functions tienen identidad propia, no se disfrazan como miembros de una clase.

## 5. Contratos 020–028: genéricos, conversiones y trabajo por callable

- Templates, aliases y lambdas genéricas se descubren antes de materializar especializaciones.
- Inferencia combina argumentos explícitos, parámetros, receiver, expected type, lambdas, packs y constantes.
- La especialización conserva origen, visibilidad, identidad estable y dependencias; los clones no pierden
  metadata semántica.
- Restricciones se normalizan como átomos comparables y participan en viabilidad y dominancia.
- Conversiones cubren escalares, referencias, tuples, functions, arrays, ownership y C variadic promotion.
- El evaluador entero conserva width/signo, normalización modular y traps de división, shift y overflow definido.
- El análisis paralelo produce shards aislados. El merger remapea ordinales y el reducer publica resultados en
  orden determinista, aun cuando los workers terminen en otro orden.

## 6. Contratos 029–045: compilación y cuerpos

- `ProjectCompilation` controla jobs, provider, cancelación, progreso y sinks sin cambiar el orden contractual
  de etapas. Los modelos posteriores sólo se publican después de sus prerrequisitos.
- El build de method bodies analiza callables regulares y funciones de namespace, descubre especializaciones
  diferidas y repite waves hasta punto fijo o error.
- Scopes, locals, name bindings y definite assignment se conservan por callable y por ordinal estable.
- Joins de flow combinan moves, borrows, storage facts, non-null, rangos y variante activa de manera conservadora.
- Statements registran control observable; switch/match/loop expresivos tienen bindings propios.
- Excepciones, using y ownership generan regiones de cleanup y continuaciones explícitas.
- El análisis de expresiones cambia a postorden iterativo desde la profundidad codificada y conserva evaluación
  izquierda-a-derecha. El cache incluye el contexto que cambia permisos y resultado.
- Calls mapean positional/named/default/pack/variadic, clasifican conversiones y seleccionan un único mejor
  candidato; ambigüedad permanece error.
- `&&`, `||` y coalescing conservan short circuit; shifts usan el tipo promovido izquierdo; pointer arithmetic
  no admite `void*` ni pointees incompatibles.
- Assignment distingue almacenamiento directo, property/indexer, move-only, ownership, views y deconstruction.
- Optional y Result se reconocen por contratos estructurales exactos, no sólo por nombre parecido.
- Escape summaries se propagan mediante worklist y se validan después contra argumentos que retiene el callee.
- Lambdas enrutan captures anidados y calculan `Fn`, `FnMut` o `FnOnce`; delegates y events conservan ABI y
  restricciones de receiver.
- Core APIs se reconocen por identidad y firma exactas. Memory orders, atomics y SIMD tienen matrices propias.
- El dumper expone una vista, no una serialización completa: los campos que omite no pueden reconstruirse desde
  su texto.

## 7. Contratos 046–055: modelos auxiliares y construcción

- El evaluador de escape usa frames y continuaciones explícitas para no consumir el stack del proceso.
- Construction model resuelve argumentos, accesibilidad, `this/base`, defaults, ownership y ciclos antes de
  publicar un plan. Los planes se buscan por `constructionOrdinal`, no por posición del vector.
- Destruction model deriva drop y cleanup de member/ownership; no inventa destrucción para valores triviales.
- Object layout aplica ABI, alignment, padding, bases, fields, variantes y discriminantes de forma determinista.
- Comptime mantiene cache por identidad/digest, límites de ejecución y diagnósticos authored separados de los
  diagnósticos del host.
- Macro expansion conserva origen y corta la expansión al límite configurado.
- Metaprogramming y opaque results publican modelos posteriores sólo con dependencias válidas.
- La política de inline diferencia permit/request/require/out-of-line; el explorador de costo recorre el grafo
  alcanzable y conserva cancelación y límites.

## 8. Contratos 056–063: HIR y MIR

- HIR consume bindings semánticos; no debe repetir resolución. La construcción es de un solo uso y fusiona
  callables paralelos en orden de ordinal remapeando locals, cleanups y regiones sintéticas.
- El lowering preserva orden de evaluación, short circuit, EH, ownership, aggregates, captures, Core APIs y
  rangos de error.
- Imports C, namespace functions y callables precompilados mantienen identidades físicas distintas.
- MIR fija ABI, layouts, dispatch, interface bindings, generics, statics, cleanup y entry point.
- La consulta de allocation plan usa `constructionOrdinal`, ya que no toda construcción crea allocation.
- Los dumpers HIR/MIR son oráculos de texto parciales y deben contrastarse además con el modelo estructural.

## 9. Hallazgos conservados sin implementación

1. `ProjectMethodBodyModel::isCompileTimeConstantExpression` usa una lambda recursiva.
2. `ProjectMethodBodyModel::isAccessorDestinationReevaluable` recorre expresiones recursivamente.
3. La deconstruction tuple usa un `std::function` recursivo para enlazar targets anidados.
4. `elementTypeHasTrivialLifetime` desciende recursivamente por tuples.
5. Otras familias ya registradas mantienen recorridos recursivos; esta fase sólo los caracteriza.
6. `ProjectOopHirModel` reconoce claves enteras de 128 bits, pero el evaluador de `static_assert` materializa
   máscaras y valores con `std::uint64_t`.
7. `verifyOutParameterAssignments` comprueba que exista una asignación, no que toda ruta de retorno asigne.
8. El resolver de divisor cero reconoce literals, constants de miembro y const locales directos; no pliega toda
   expresión const derivada.
9. Los dumpers omiten campos internos deliberadamente.
10. La promoción de objetos de clase al stack es post-HIR, conservadora y limitada a un MiB.
11. Diagnósticos y markers conservan textos históricos `Stage`, `Macro` y ediciones internas.

Cada punto es `HALLAZGO NO AUTORIZADO`: no se corrigió ni se convirtió en trabajo de producto.

## 10. Reconstructibilidad

El componente es reconstructible para la revisión congelada porque los 121 archivos y 1,392 símbolos quedan
ligados a uno de 63 contratos, y cada contrato tiene casos diseñados con entrada, error, oráculo, capa y
frecuencia. El dictamen no significa que esos casos estén implementados o ejecutados, ni que exista cobertura
instrumental.
