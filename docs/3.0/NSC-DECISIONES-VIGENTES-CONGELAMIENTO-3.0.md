# NativeSharp 3.0 — decisiones vigentes del congelamiento

Fecha de reconciliación: 2026-10-26.

Este archivo es la puerta de entrada operativa al estado presente del congelamiento 3.0. No reemplaza los
documentos de evidencia de `docs/3.0/decisiones-congelamiento/`; los indexa y separa lo vigente de lo histórico.

## 1. Autoridad y uso

Orden de autoridad para continuar el trabajo:

1. decisiones explícitas más recientes de Alberto Huerta Luna;
2. código, pruebas y commits de la rama actual;
3. este registro;
4. documentos de decisión enlazados desde este registro;
5. documentos anteriores, únicamente como evidencia histórica.

Un documento histórico no autoriza restaurar una conducta retirada. Si contradice este registro, se conserva
como historia y se corrige su afirmación de estado cuando corresponda.

Antes de cada commit se debe registrar en la nota de trabajo:

```text
HEAD:
Elemento:
Contrato vigente:
Documento histórico que podría contradecirlo:
Archivos exactos que se modificarán:
```

Una contradicción detiene sólo el elemento afectado. No se resuelve eligiendo el documento más antiguo ni
ampliando el cambio.

## 2. Instantánea auditada

| Campo | Valor |
|---|---|
| Rama | `development-release-candidate` |
| Base de la campaña | `63aba0be4` |
| HEAD auditado | `403b066417139b9755e3fa711be700c07151974c` |
| Commits desde la base | 53 |
| Worktree al auditar | limpio |
| Estado del HEAD | contiene P8 con versionado minor de NSMETA; decisión reemplazada y pendiente de retirar |
| Batería completa | no ejecutada para esta campaña |
| Unitarias completas | pendientes de la corrida final de Alberto |
| Árbol histórico | pendiente de la corrida final |

El conteo `68/104` pertenece al instante `b1f1a9699`. Después se agregaron veinte commits hasta el HEAD auditado,
pero no existe en la rama un tracking canónico que asigne esos commits uno a uno a los elementos 69–88. Por eso
este registro no publica un avance numérico inferido. El siguiente trabajo determinista es reemplazar P8 y luego
continuar con el bloque declarado como elementos 89–99.

`docs/3.0/NSC-ESTADO-CONGELAMIENTO-3.0.md` no es autoridad operativa mientras siga afirmando que el compilador
validado es `73b03a394` y que el plan terminó 108/108. Debe reconciliarse al cierre documental.

## 3. Estados

- `VIGENTE`: contrato aprobado y presente en la rama.
- `REEMPLAZADO`: implementación o afirmación que ya no representa la decisión actual.
- `PENDIENTE`: trabajo todavía necesario antes del cierre indicado.
- `DIFERIDO`: rechazo explícito y seguro aceptado para una etapa posterior.
- `PROHIBIDO`: mecanismo que no debe reintroducirse.
- `NO VERIFICADO`: no existe evidencia final suficiente para declararlo cerrado.

## 4. Contratos vigentes

| Contrato | Decisión vigente | Implementación principal | Evidencia detallada |
|---|---|---|---|
| C-08 / DEC-09 | `IAllocator` tiene una sola firma con tamaños `long` y alineación explícita. La firma 3.0.4 se rechaza con diagnóstico de migración. | `6758083f5`, `7f6214d00` | `32-P6-IALLOCATOR-FIRMA-UNICA.md` |
| C-06 | `NSLANGSURFACE 3.0` publica lo implementado; list patterns son de `match`/`is`, no de etiquetas `case`. | `73150c308`, `30959b2a8`, `1bde9d387` | `04-DEC-07-08-SUPERFICIE-C11-C14.md`, `05-C-06-ORACULOS-SUPERFICIE.md` |
| C-07 | `NativeSharp.Core.IDisposable` es parte de la biblioteca oficial y `using` dispone una vez por salida. | `7b4f91771` | pruebas de `Freeze30ContractTests` |
| C-01 / DEC-02 | Todo `new` debe alcanzar un dueño. Un local no dueño es error. `throw` transfiere la excepción al mecanismo de manejo. | `ea0da601c`, `8632a809e` | `02-DEC-02-TL-LANG-339-RAII.md` |
| C-02 / DEC-03/04 | Un productor conocido que puede suspender no admite `await` síncrono. Un origen desconocido se permite; si queda `Pending` tras el único poll, hace pánico. | `ea0da601c`, `6d941cfcc`, `8c1559649` | `09-AWAIT-SIN-RECURSION.md`, `10-D04-ORIGEN-DESCONOCIDO.md` |
| C-05 / DEC-05 | Un `.ns` usa un proyecto implícito y el pipeline 3.x. Las nueve acciones CFG/dominancia/SSA/opt inspeccionan LLVM, no el pipeline 1.x. | `2e154f127`, `30fe9b7e7` | `11-DEC05-INSPECCION-LLVM.md`, `NSC-FORMATOS-INSPECCION-LLVM.md` |
| C-11 / DEC-11 | Los structs con destructor o campos `owned` son move-only. En arreglos, cada slot tiene estado vivo/muerto; sólo los vivos se destruyen. | `4d6237d84`, `b1f1a9699` | `15-DEC11-SLOT-VIVO-MUERTO.md` |
| C-12 | Los arreglos propietarios destruyen sus elementos vivos antes de liberar el bloque; las colecciones deben respetar ese contrato. | `0b6da967f`, `01f408b04` | estado y sondas de C-12/DEC-11 |
| DEC-06 | `delete[]` de un local `owned T[]` consume el ownership, deja el local muerto y evita la limpieza duplicada. | `01e37532e` | `12-DEC06-DELETE-OWNED-ARRAY-LOCAL.md` |
| C-13 / P-1 | Toda llamada que declara `throws`, todo `throw` y todo `throw;` que escapan deben atraparse o declararse. | `8d46bf5cc`, `73b03a394` | `33-DEC14-TYPED-THROWS-DOC.md` |
| DEC-10 / P10 | La excepción atrapada es un préstamo del `catch`; no escapa. Sólo `move(e)` transfiere ownership. Los parámetros de excepción son préstamos no escapables. | `93bda0690`, `c8b8f42bc`, `2a65e3c59` | `13-DEC10-EXCEPCION-ATRAPADA.md`, `30-*`, `31-*` |
| C-14 | Los futures son move-only; `await` consume el future y un future incompleto libera su frame. | `717c9e5f9` | pruebas de C-14 y manifiesto de superficie |
| C-16 | Los generadores destruyen exactamente los locales inicializados y ejecutan cleanup activo durante unwind. | `0f182413c` | pruebas de C-16 |
| C-17 / DEC-15 | Los errores de generadores async llegan a `MoveNextAsync`; el poll reanuda sin repetir trabajo y la propagación se prueba sin depender de WER. | `bc8fca456`, `bf4834cf3`, `f648ba11e`, `ba91ef419`, `6a5dc1a10` | `03-DEC-15-TL-LANG-376-PROPAGACION.md`, `14-D01-REPETICION-AWAIT.md` |
| DiagnosticId | Los IDs históricos conservan su valor; los nuevos se agregan al final. | `88982f3ab` | `01-DIAGNOSTICID-VALORES-HISTORICOS.md` |
| D-10 / DEC-01 | `nsc` no escribe `error*.log` automáticamente. `--error-log` lo solicita. No se borran archivos externos desde esta tarea. | `4bf4a7a18` | `07-D10-ERROR-LOG-OPCIONAL.md` |
| D-09 / DEC-13 | La validación física de imports contra DLL del host es opcional y está apagada por defecto. | `88c1246c4` | `08-DEC13-VALIDACION-DLL-OPCIONAL.md` |
| DEC-12 / P1 | `where T : copy` puede restringir el parámetro del tipo en métodos, constructores y accesores; se valida al usar el miembro. | `62ad32baa`, `a3e8ad5fd`, `08bd66185` | `16-*`, `17-*`, `26-*` |
| DEC-12 / P2 | Las APIs con delegado por valor exigen `copy`; existen delegados `In*`; ordenar, invertir y remover reubican mediante intercambio. | `737503602` | `18-P2-DELEGADOS-IN-Y-TRUEQUE.md` |
| DEC-12 / P3 | El lenguaje y la biblioteca exponen referencias mutables o readonly sin copiar valores move-only; una copia inevitable exige `copy`. | `b0a03bd98`..`e26ddb4bd` | `19-*` a `28-*` |
| DEC-11 / P4 | Un `Span` derivado de un arreglo conserva su mapa de vida y desplazamiento; leer muerto atrapa y escribir revive. | `80b186758` | `29-P4-SPAN-CON-MAPA-DE-VIDA.md` |
| P9 | `C12LinkedList` maneja el `throws` de `RemoveFirst` y conserva el resultado 1123. | `e3e451f86` | cambio focal en `Freeze30ContractTests` |
| DEC-14 | `typed-throws.md` describe la conducta medida del compilador en ediciones 2 y 3. | `b8585f145` | `33-DEC14-TYPED-THROWS-DOC.md` |

## 5. Reemplazado o retirado

| Elemento | Estado actual |
|---|---|
| `403b06641` y `34-P8-EPOCA-NSMETA.md` | `REEMPLAZADO`. Deben retirarse completos: minor 41, `verifyLayoutEpoch` y el golden 1.40 no son el mecanismo aprobado. |
| NSMETA minor como época de layout | `PROHIBIDO`. No se usa la versión minor de NSMETA como gate de compatibilidad. |
| Pipeline 1.x de archivo | `REEMPLAZADO`. No se restaura; las acciones públicas observan LLVM 3.x. |
| Validación obligatoria contra DLL del host | `REEMPLAZADO` por `--validate-system-imports`, opcional. |
| Repetición indefinida de un `await` síncrono pendiente | `PROHIBIDO`; el contrato actual hace un poll y pánico si el origen aceptado queda pendiente. |
| Rechazo de `delete[]` para todo local `owned T[]` | `REEMPLAZADO`; DEC-06 permite consumir el dueño. |
| Límite DEC-10 que permitía escapar mediante un método | `REEMPLAZADO` por P10: todo parámetro de excepción es préstamo no escapable. |
| Restauración byte por byte indiscriminada de los históricos | `PROHIBIDO`; se conservan sólo los bloques indispensables para contratos aprobados. |
| Afirmaciones actuales de `NSC-ESTADO-CONGELAMIENTO-3.0.md` sobre 108/108 y `73b03a394` | `REEMPLAZADAS`; permanecen como fotografía histórica hasta su corrección final. |

## 6. Pendientes anteriores al cierre

### 6.1 Siguiente acción determinista

1. Revertir completamente `403b06641`.
2. Marcar la frontera incompatible mediante `objectAbiVersion = 3`, no mediante NSMETA minor.
3. Rechazar una biblioteca con Object ABI 2 mediante un diagnóstico que pida recompilar.
4. Actualizar `Stage48_2` para que los goldens 1.15–1.40 esperen ese rechazo.
5. Reemplazar el documento 34 por la evidencia de Object ABI 3.

Este cambio es una frontera del ABI de objetos. No autoriza versionado variable de NSMETA.

### 6.2 Defectos focales declarados

- Un campo `Future<int>` de clase se rechaza con `field type has no resolved named-type layout`.
- `f078` en modo biblioteca falla con `value drop has no address/member model`.
- `Use(new Box(3))` no destruye el temporal en código síncrono.
- Verificar y corregir, si fallan, `await F(await G())`, el `await` como valor final de una expresión de bloque y
  el operando `owned` guardado en el frame cuando se abandona la corrutina suspendida.
- Verificar `Dictionary`, `HashSet`, `SortedSet`, `SortedList`, `PriorityQueue` y las demás colecciones con valores
  destructibles.
- Verificar punteros, `fixed` e interop con tipos con destructor.
- Resolver cualquier defecto reproducido dentro de esos puntos mediante un commit causal y una prueba focal.

### 6.3 DEC-16 e históricos

- Auditar los 244 archivos históricos modificados desde la base.
- Conservar sólo los bloques mínimos exigidos por C-01, C-05, C-08, C-11, C-13/P-1 y C-14, además de cualquier
  contrato posterior aprobado que tenga una relación causal demostrada.
- Restaurar byte por byte todo bloque restante.
- Restaurar los `delete[]` retirados por P-3, porque DEC-06 los vuelve válidos.
- No restaurar archivos completos si contienen simultáneamente cambios indispensables y cambios ajenos.

### 6.4 Documentación pendiente

- Corregir `docs/3.0/language/async-generator-errors.md`: retirar la afirmación «C-13 exige declararlo» si no
  coincide con la conducta medida.
- Reescribir `NSC-ESTADO-CONGELAMIENTO-3.0.md` contra el HEAD final.
- Localizar o reconstruir el tracking canónico de 104 elementos; no publicar un conteo inferido.
- Actualizar el plan de congelamiento al final, después del código y de las pruebas focales.

### 6.5 Medición y verificación

- D-08: medir tamaño y vida de frames de corrutina; es medición y no cambia contratos por sí sola.
- D-12: verificar bibliotecas precompiladas mediante Object ABI 3.
- Verificar la instalación sólo cuando Alberto autorice el gate correspondiente; esta tarea no instala ni despliega.

## 7. Diferido después del congelamiento

Estos elementos sólo permanecen diferidos si la prueba focal confirma rechazo explícito y seguro, sin
miscompilación, fuga, ABI inconsistente ni cleanup omitido:

- C-09: delegado genérico cuyo retorno usa su propio parámetro.
- C-10: `where` en funciones genéricas globales.
- C-15: superficie de `Result/?` a través de `await` que falle cerrada; cualquier defecto de unwind no se difiere.
- C-18: null-conditional write para indexadores de usuario y bit-fields.
- C-19: conversiones e inferencia de vida de buffers y vistas.
- C-20: superficie moderna de `params` fuera de `ReadOnlySpan`.
- C-21: imports de metadata con tipos declarantes genéricos, mientras el rechazo sea explícito.

Si cualquiera acepta código y luego miscompila, pierde cleanup o mezcla ABI, deja de ser diferido y bloquea el
congelamiento.

## 8. Prohibiciones operativas vigentes

- No usar NSMETA major/minor como época de layout.
- No reactivar el pipeline 1.x.
- No validar contra DLL del host por defecto.
- No borrar ni modificar archivos de `C:` como parte de esta campaña.
- No cambiar NSC Builder ni infraestructura ajena.
- No ejecutar baterías, suites completas, baseline, instalación ni deploy durante los elementos focales.
- No modificar una prueba existente salvo autorización explícita vinculada al contrato que reemplaza su oráculo.
- No presentar una sonda focal, una compilación o un documento como certificación de la batería completa.

## 9. Gates finales, todavía no ejecutados

1. Pruebas focales de cada corrección en el target exacto.
2. Suite completa de NSC Builder 3, una sola vez al final y ejecutada por Alberto.
3. Unitarias, incluido `Freeze30ContractTests`, ejecutadas por Alberto.
4. Árbol histórico de 349 proyectos y 52 casos contra el compilador de referencia.
5. Reconciliación final de manifiesto, ABI, NSMETA, documentos de estado y tracking.

Hasta completar esos gates, el estado correcto es `IMPLEMENTADO CON VALIDACIÓN FOCAL`, no `CONGELADO` ni
`CERTIFICADO`.
