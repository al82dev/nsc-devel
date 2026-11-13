# Regla conservadora de vida para referencias guardadas en arreglos

Rama `development-main-fixes`. Campaña: mejora "referencias de clase guardadas en arreglos" de
`X:\Examples-NSC\Problemas\mejoras.md`. Evidencia: `A:\nsc-main-fixes-evidencia\claude\logs`
(`conservadora-*`), `A:\nsc-main-fixes-evidencia\claude\probes\RESUMEN-conservadora.txt` y
`A:\nsc-fixes-libraries\build\compiler-check` (bibliotecas compiladas, sólo lectura).

## Decisión del usuario (13 de noviembre de 2026)

> Usar un error conservador cuando no podamos demostrar que la referencia sigue siendo válida en el momento
> de usarla. Prefiero rechazar algunos programas seguros antes que aceptar silenciosamente un posible acceso
> a memoria liberada. La regla debe aplicarse al uso de la referencia, no convertir todos los estados
> `Unknown` en error.

- Destrucción demostrada: error de referencia a objeto destruido.
- Vida no demostrada después de una transferencia o posible destrucción: error distinto.
- Propiedad transferida y objeto demostrado vivo: se permite el uso.
- Referencia almacenada que nunca se usa después: no es error.
- Un `move` por sí solo no es una destrucción: se sigue hasta el nuevo dueño; si no se demuestra que el
  objeto vive cuando se usa la referencia, aplica el error conservador.

La decisión cambia qué programas compilan; el usuario la autorizó expresamente y pidió probar tanto los
rechazos como los programas válidos que deben seguir aceptándose.

## SCOPE LOCK

- Objetivo: aplicar la decisión al replay de referencias en arreglos sin retirar ningún diagnóstico preciso.
- Naturaleza: funcional (contrato de seguridad de vida de la mejora 2).
- Debe permanecer idéntico: todo callable que el replay no visita; los diagnósticos precisos de receptor,
  campo, copia, ciclos y prueba tardía; las lecturas sin evento de vida previo.
- Base de la campaña: 152654e5a0e2d84d4c2f245ea7590488f0497430; revisión de partida: 26c88d830.
- Archivos modificados: `include/nativesharp/Frontend/ArrayReferenceFlowState.h`,
  `include/nativesharp/Frontend/ProjectMethodBodyModel.h`,
  `src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp`,
  `src/Frontend/Bodies/MethodBodyBindingRecorder.cpp`, `src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp`,
  `src/Frontend/Bodies/StatementAnalyzer.cpp`, `src/Frontend/Bodies/AssignmentAnalyzer.cpp`,
  `src/Frontend/Bodies/ArrayReferenceLatePathProof.cpp`, `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp`,
  este documento.
- Protegidos: ejemplos originales, batería histórica, `C:\native-sharp`, `A:\nsc-limitaciones-vistas`,
  interoperabilidad C/C++, `A:\nativesharp-libraries`.
- Prohibido: merge, rebase, push, despliegue, batería completa, reconstrucción completa repetida.

## Contrato

Dominio: un callable que el replay visita, sus arreglos nativos locales creados con `new` (y sus alias
locales) y sus dueños locales creados con `new`. Fuera de ese dominio no cambia nada.

1. **Callables visitados.** Además de los candidatos anteriores, todo callable que declara un arreglo
   nativo local y mueve un dueño local a un parámetro `owned`, o que declara un dueño con `new` en un bloque
   anidado. Ya no se exige la cobertura lineal: lo no modelado sólo amplía la incertidumbre.
2. **Uso.** Toda lectura de un elemento de un arreglo rastreado es un uso, salvo un operando directo de `==`
   o `!=`, el destino de una asignación `=`, y el receptor de `elemento.Campo = X` cuando `X` es una llamada
   que nunca retorna (el valor se evalúa antes de guardar).
3. **Eventos de vida.** Se guardan aparte de los estados de lectura (`lifeEnded`, `lifeMaybeEnded`), porque
   `markUnknown()` degrada esos estados por motivos que no terminan ninguna vida:
   - limpieza de un dueño único y presente (efecto `Destroyed`, fin de bloque): fin cierto;
   - limpieza de un dueño unido o ausente en algún camino: fin posible;
   - `move` de un dueño a un parámetro `owned` cuyo efecto no se demuestra (callee virtual, de interfaz, no
     resuelto, sin resumen o de efecto desconocido): fin posible, `releaseUnproven`. No es una destrucción;
   - unión de ramas: fin en ambas ramas es fin cierto; en una sola, fin posible.
4. **Candidatos de un uso.** Los objetos guardados en la ranura leída, en ranuras de índice desconocido del
   mismo arreglo, en cualquier ranura cuando el índice leído es desconocido, y los objetos expuestos a código
   no modelado cuando el arreglo también llegó a ese código (una llamada sin resumen que recibe la referencia
   y el arreglo puede guardar una en otro).
5. **Veredicto.**
   - Ningún candidato con evento de vida: se acepta (el dueño sigue vivo, se movió a un local vivo, o el
     elemento guarda otro objeto).
   - Un único candidato, ranura exacta (índice literal, sin escrituras desconocidas, sin índice especializado
     ni potencial) y fin cierto: `reference to a destroyed object cannot be used`.
   - En otro caso: `cannot prove that the referenced object is still alive when this array element is used`.
   - Un uso ya reportado por una prueba precisa no se repite.
6. **Alcanzabilidad.** Un uso que no se alcanza no es un uso. El estado del replay se marca *detenido*
   (`stopped`) después de una operación sin salida normal, y las uniones ignoran el camino detenido:
   - llamada directa resuelta no virtual a un callable de `arrayReferenceNonReturning_`: cierre de
     `callablesWithoutNormalExit_` sobre sentencias de primer nivel que no terminan (llamadas, `delete` de un
     tipo cuya cadena de destructores no retorna, `new T()` con constructor o inicialización de tipo que no
     retorna, `while (true)` sin `break`, `return` ni `throw`), si ninguna sentencia de primer nivel anterior
     contiene un `return`;
   - destrucción por efecto `Destroyed` o por fin de bloque de un objeto cuyo tipo creado tiene una cadena
     de destructores que no retorna;
   - método estático o constructor de un tipo cuya inicialización estática no termina (constructor estático
     sin salida normal, o campo estático inicializado con una llamada no calificada al único método estático
     de ese nombre, que no retorna);
   - cuerpos de ciclos que nunca se ejecutan: `while (false)`, `bool x = false; while (x)` (declaración justo
     antes) y `for (int i = A; i OP B; ...)` con literales cuya primera prueba es falsa.
   La regla conservadora no usa la heurística anterior de "inicialización estática no trivial": eso no
   demuestra que la lectura no se alcance.
7. **Seguir el move hasta su nuevo dueño.** Un `move` del dueño a un método de instancia de fuente, no
   virtual, cuyo cuerpo completo es `F = move(p);` (o `this.F = move(p);`) con `F` un campo `owned` de
   instancia de su clase, entrega el objeto al objeto receptor; si el receptor es un local rastreado, el
   objeto queda *adoptado* por él (`adopt`) y vive mientras ese dueño viva. Una llamada posterior que recibe
   al dueño adoptante (como receptor o argumento) o una escritura de campo a través de él puede reemplazar el
   campo: lo adoptado pasa a fin posible (`releaseAdopted`). La limpieza del dueño adoptante con varios
   objetos es un fin posible.
8. **Alias.** Reasignar un alias local de arreglo deja de asociarlo a las ranuras del arreglo anterior; una
   raíz reasignada sin alias deja de rastrearse.
9. **Orden.** La regla corre al final de la fase de construcciones (`reportArrayReferenceLatePaths`),
   después de todas las pruebas precisas.

## Semántica de ausencia

Un callable sin arreglos nativos locales, o sin dueños locales transferidos o destruidos por ámbito, no
entra al replay por esta regla y no recibe diagnósticos nuevos. Un callable visitado sin eventos de vida
sobre objetos guardados tampoco.

## Pruebas (`nsc.ns3.t06.escape-summaries`)

Controles nuevos (`unprovenControl`): rechazo de drop virtual, drop precedido de llamada, guardado en dos
niveles, rama de runtime (receptor y copia), índice calculado, adopción seguida de una llamada al dueño
adoptante y adopción por un dueño cuyo bloque terminó; aceptación de move a un local vivo, adopción por un
dueño vivo, referencia guardada sin uso, elemento vivo ajeno, comparación con `null`, lectura antes del
evento y lectura tras una llamada que no retorna.

Resultados (compilador de la copia, compilación incremental): `nsc.ns3.t06.escape-summaries`,
`nsc.stage50_7.borrow-escape-analysis`, `nsc.class-value-origins` y `nsc.stage56.c7-definite-assignment`
pasan (`claude\logs\conservadora-final-*.log`).

Sondas `claude\probes` (sólo análisis, `--check-project-constructions`, nunca se ejecutaron):

- pasan a rechazarse con el error conservador: P10 (índice calculado), P11 (rama de runtime), P14 (guardado
  en dos niveles), P16 (delete virtual), P17 (delete precedido de una llamada);
- siguen aceptadas las válidas: P05, P06, P07 (move a un parámetro que conserva el objeto: ahora por
  adopción), P08b, P09b y P20;
- conservan su diagnóstico preciso: P01 (ejemplo original), P02, P04, P13, P19, P21 y P22; P03, P08, P09 y
  P12 siguen rechazadas por otra regla (copia o move de un dueño RAII);
- siguen aceptadas sin diagnóstico: P18 (arreglo guardado en un campo, fuera del dominio).

Compilación de las 32 bibliotecas de `A:\nsc-fixes-libraries` en O0 con este compilador (sólo lectura, salida
en `build\compiler-check\out-conservadora`): 31 pasan sin diagnósticos nuevos. `NativeSharp.Transfer` falla
con `Stage 37 const initializer ... could not be materialized` también con el compilador anterior
(26c88d830) sobre el mismo árbol: ese repositorio tiene cambios sin commit de otro carril en
`NativeSharp.Transfer`, así que ese resultado no es atribuible a esta regla y queda NO VERIFICADO.

### Expectativas reemplazadas por la decisión

Estos controles de la campaña se aceptaban porque no existía prueba precisa de camino muerto; sus fuentes no
cambiaron. Ahora deben rechazarse con el error conservador (`conservativelyRejected`):

- algún camino puede destruir el objeto: `integration_override`, `integration_while_conditional_break`,
  `integration_loop_conditional_break`, `integration_loop_alias_conditional_break`,
  `integration_while_parameter_false`, `integration_while_parameter_no_call`,
  `integration_while_parameter_caller_spin`, `integration_while_parameter_mutated`,
  `integration_while_parameter_caller_static_init`, `integration_while_local_changed`,
  `conditional_delete_nonreturning_branch`, `linear_forwarded_delete_cycle` y los fixtures
  `late-reference-field` y `late-external-reference` (sus comprobaciones de testigo siguen ejecutándose);
- el índice no está demostrado: `integration_read_index_reassigned`, `integration_read_index_ref`,
  `integration_parameter_read_changed`, `integration_parameter_read_constrained`,
  `integration_restricted_parameter`, `integration_mutated_parameter`, `integration_reference_parameter`,
  `integration_mutable_index`, `integration_aliased_index`;
- el guardado sale de límites antes del uso (la regla no modela longitudes):
  `integration_empty_parameter_array`, `integration_literal_out_of_bounds`,
  `integration_loop_alias_out_of_bounds`.

Los programas de los grupos segundo y tercero son seguros en ejecución; su rechazo es el costo que la
decisión acepta. Ninguna prueba histórica (anterior a la campaña) se modificó.

## Límites conocidos (no cubiertos, no afirmados como seguros)

- Un `move` del dueño hacia un campo o un static desde el propio callable, un retorno, y `delete` de un
  local `owned`, no se registran como eventos de vida. La adopción sólo reconoce el cuerpo exacto
  `F = move(p);`.
- La no terminación se demuestra sólo por las formas listadas en el punto 6; otras (recursión, ciclos con
  condición calculada) no detienen el camino y pueden producir el error conservador en programas seguros.
- Ciclos: un evento en una iteración y un uso al inicio de la siguiente se resuelven con la pasada de
  ciclos del documento 06 (M2-05).
- Arreglos y referencias en campos de objetos locales: cubiertos por el documento 07 (M2-06). Arreglos
  recibidos como parámetro y colecciones (`List<T>`) siguen fuera del dominio: su contenido es
  responsabilidad del programador según `known-life-sources.md`.
- La regla corre en `--check-project-constructions` y en la compilación completa; la fase
  `--check-project-method-bodies` por sí sola no la ejecuta.
