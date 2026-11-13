# Referencias guardadas en campos de receptores locales (M2-06)

Rama `development-main-fixes`. Extiende la regla conservadora de los documentos 04 y 06. Evidencia:

- `A:\nsc-main-fixes-evidencia\claude\logs\m2-06-*`;
- `probes\RESUMEN-m2-06.txt` (P18 y P25);
- `logs\libs-out-m2-06.log` (control de bibliotecas).

## Decisión

El usuario delegó en Claude las decisiones de alcance. Con esa delegación, Claude extendió la regla de vida
a esta ruta: una referencia de clase que un método guarda en un campo de un objeto local, y que otro método
de ese objeto usa después. Se cubren dos formas de campo:

- arreglos nativos de referencias de clase en campos (P18, el caso que el usuario señaló como «arreglo en
  campo»);
- campos de referencia simples (P25), porque son la misma ruta con una sola ranura.

Antes, ambas sondas compilaban sin diagnóstico, con un uso posible de memoria liberada. Ahora se rechazan.
La garantía del contrato normativo (`known-life-sources.md`, límite 1) no cambia para referencias que llegan
por parámetros o campos de objetos que no son locales del llamador. Esas siguen bajo responsabilidad del
programador.

## SCOPE LOCK

- Objetivo: que el replay siga referencias guardadas en campos de objetos locales del llamador.
- Naturaleza: funcional (agrega rechazos E1/E2).
- Debe permanecer idéntico:
  - los veredictos de arreglos locales;
  - los callables sin acceso a campos de referencia de receptores locales;
  - los campos `owned` y `static`.
- Archivos modificables:
  - `ArrayReferenceFieldEffects.cpp` (nuevo), `CMakeLists.txt`;
  - `ArrayReferenceFlowState.h`, `ProjectMethodBodyModel.h`;
  - `MethodBodyBindingRecorder.cpp`, `MethodBodyCallableLifecycle.cpp`;
  - `NativeSharp3T06EscapeSummaryTests.cpp`;
  - este documento.
- Prohibido: merge, rebase, push, despliegue, batería completa, reconstrucción completa repetida.

## Contrato

1. **Campos rastreados.** Campos de instancia no `owned` cuyo tipo es una referencia de clase, o un arreglo
   nativo de referencias de clase. Se excluye `string`.
2. **Resumen por método de instancia** (`arrayReferenceFieldEffects`):
   - **Store.** `F = p;`, `this.F = p;` o `F[i] = p;`, donde `p` es un parámetro. El índice se toma de un
     literal; un campo simple es la ranura 0.
   - **Use.** `F` o `F[i]` en cualquier otra posición.
   - **No es uso:**
     - un operando de `==` o `!=`;
     - `F.Length`;
     - el destino de una asignación.
   - **Unknown (más Use con índice desconocido).** Un arreglo en campo en cualquier otra posición, o
     reemplazado.
   - **Llamadas no virtuales sobre `this`.** Se componen: el Store se traduce por argumentos que son
     parámetros directos.
   - **UnknownAll.** Una llamada virtual o no resuelta sobre `this`, una lambda, o `this` en otra posición.
   - **Efecto cierto.** El efecto está en una sentencia de primer nivel, fuera de `?:`, `&&`, `||` y `??`,
     y no hay `return`, `throw`, `break` ni `continue` en una sentencia de primer nivel anterior.
3. **Replay del llamador.** Cada par (objeto receptor rastreado, campo) es un pseudoarreglo con la misma
   regla de vida del documento 04. El objeto, y no el local, identifica el par; por eso `raii B b2 = move(b)`
   conserva el contenido.
   - **Receptores que no son un único dueño rastreado** (alias, uniones) usan un receptor desconocido. Sus
     guardados exponen el valor y vuelven escapado el campo en todo receptor. Sus llamadas sin resumen
     vuelven escapado todo campo.
   - **Llamadas sobre locales.**
     - Primero se evalúan los usos del callee. Juntos forman una observación sobre la llamada.
     - Después se aplican los guardados: los ciertos con escritura cierta y los demás con escritura
       posible.
     - Si el callee no tiene resumen (virtual, de interfaz, no resuelto) o tiene UnknownAll, la llamada usa
       todo campo del receptor con índice desconocido (incluido un campo centinela) y el receptor escapa.
   - **Rutas de escape.**
     - Un objeto rastreado pasado como argumento escapa como receptor.
     - Una llamada que recibe dos o más objetos rastreados (receptor incluido) los expone a todos.
     - Un guardado en un receptor que no es local (parámetro, campo, resultado) expone el valor.
   - **Acceso directo `local.F` desde el llamador.** Es un uso con índice desconocido; el receptor escapa y
     todo objeto rastreado queda expuesto.
4. **Veredicto.**
   - E1 «reference to a destroyed object cannot be used» solo con destrucción cierta y uso cierto en el
     callee.
   - En otro caso con evento de vida, E2 «cannot prove that the referenced object is still alive when this
     call uses a stored reference».
   - Para los pseudoarreglos, la bandera global `unknown` no impide la exactitud, porque todos sus
     escritores se modelan o los vuelven escapados.
5. **Ciclos.** Las observaciones de campo participan en la pasada del documento 06, con `mustWritten`
   cuando hay un único uso.
6. **Candidatos del replay.** Llamadores con `move` o un dueño anidado que llaman sobre un local a un método
   con efectos de campo o sin resumen, o que acceden directamente a un campo rastreado de un local.

## Pruebas (`nsc.ns3.t06.escape-summaries`, 447 comprobaciones, pasa)

- **E1:**
  - `field_array_destroyed`, `field_plain_destroyed`;
  - `field_scope_exit`, `field_through_this_call`;
  - `field_moved_receiver`.
- **E2:**
  - `field_unproven_virtual_drop`, `field_virtual_receiver_call`;
  - `field_direct_access`, `field_receiver_passed`;
  - `field_loop_previous_iteration`.
- **Aceptados:**
  - `field_live_owner`, `field_comparison_only`, `field_other_slot`;
  - `field_never_used`, `field_loop_same_iteration`.
- **Sondas:**
  - P18 se rechaza con E1 (`Main.ns:28:16`);
  - P25 se rechaza con E2 (`Main.ns:26:16`);
  - el resto no cambia.
- **Suites dirigidas:** `nsc.stage50_7.borrow-escape-analysis`, `nsc.class-value-origins` y
  `nsc.stage56.c7-definite-assignment` pasan.

## Límites

- **Receptores fuera del callable.** No se siguen los receptores que son parámetros o campos del propio
  llamador; sus guardados solo exponen el valor (límite 1 de fuentes de vida conocida).
- **Constructores.** Un constructor que guarda un parámetro en un campo no se resume. `new T(x)` no
  registra guardado: el argumento queda expuesto solo si otra regla lo expone.
- **Campos `static`.** Son el límite 1 del contrato normativo.
- **Falsos positivos posibles:**
  - `local.items.Length` desde el llamador cuenta como uso;
  - el receptor desconocido reúne todo valor guardado por cualquier receptor del mismo campo.
- **Diferencia sin aclarar entre P18 y P25.** P25 recibe E2 aunque la destrucción es cierta. El valor se
  guarda por un método de instancia de `Holder` cuya destrucción se registra, pero la prueba exacta no se
  cumple. No se investigó más porque el rechazo es correcto.
