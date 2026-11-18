# Origen de vistas construidas dentro de tuplas

## Alcance y frontera

Esta corrección pertenece a los casos de préstamos y vida de vistas de
`TAREAS-CHAT-VISTAS.md`. La copia es `A:/nsc-limitaciones-vistas`, rama
`development-fixes-limitaciones-vistas`; el inicio `3b5df359a` y la base
compartida `d00b3a869` no están certificados. Se conservan los ejemplos
originales y los contratos de solo lectura, lifetime y barrera para origen
desconocido.

El cambio abarca `collectViewSourceFields` en `OwnershipAnalyzer.cpp`, la
validación de `recordViewAssignmentRoot` en `AssignmentAnalyzer.cpp` y el
fixture `tests/integration/tuple_inline_view_lifetime_vistas.py`. La modificación
independiente de FnOnce en otro punto de `AssignmentAnalyzer.cpp` no pertenece
a esta corrección y debe integrarse separadamente. Ambos analizadores son
archivos compartidos que pueden entrar en conflicto al integrar.

Se protegen las demás copias y builds, C:, LLVM/Clang, herramientas externas,
interoperabilidad C/C++, Daikon y lowering/HIR async; `NativeSharp.Text` queda
aplazado. No hay merge, rebase, push ni despliegue. Las salidas permanecen en A;
la validación usa compilación incremental y pruebas dirigidas, sin batería
completa, baseline ni reconstrucción completa. El cierre requiere confirmar cada
caso asignado y su evidencia; esta corrección no cierra la campaña.

## Reproducción y causa

Una vista construida como elemento de tupla con `new View<int>(ref owner.Value)`
se desestructuraba y asignaba a un local que sobrevivía al dueño. El compilador
aceptaba ese escape y permitía reemplazar al dueño mientras la vista seguía
viva. Las reproducciones y controles fuente/binario están en
`A:/nsc-vistas-evidencia/generic-tuple-view-evidence.md` y sus JSON; los
negativos no se ejecutaron. Un control que asignaba directamente la misma vista
ya rechazaba ambos casos.

La recolección de fuentes de la tupla llegaba al constructor de la vista sin
conservar el nombre del dueño referenciado. La desestructuración propagaba lo
conocido de la tupla y la validación de asignación de una vista simple no
examinaba todas las raíces recopiladas salvo en casos condicionales. Esa
combinación dejaba sin comprobar el alcance de algunos dueños.

## Cambio y contrato

Al visitar una construcción de vista, `collectViewSourceFields` registra las
raíces locales o de parámetros identificables de sus argumentos `ref`. Recorre
todos esos argumentos y conserva el marcador desconocido ya existente: no
convierte una procedencia no demostrada en una garantía. La asignación a una
vista o tupla de vista comprueba cada raíz local conocida antes de permitir que
el valor se almacene en un ámbito más amplio.

La comprobación es conservadora: un constructor que recibe una referencia pero
no la retiene puede verse restringido. Las raíces se representan mediante nombres
y la marca desconocida no demuestra seguridad de aliases de clase, punteros,
almacenamiento global, todas las llamadas ni todos los módulos binarios. No se
introdujo API nueva, cambio de ABI ni retención dinámica de módulos. No se
añadieron llamadas recursivas. El recorrido de argumentos es lineal; la búsqueda
de duplicados utiliza la estructura de fuentes existente.

## Validación y límites

En el segundo build incremental conjunto, el nsc de vistas tuvo SHA-256
`841401f21f300d28a72a7e0f3a261a983912f1702d0b3120af4c5a251dc77b43`. La prueba
dirigida terminó con código 0 (`A:/nsc-vistas-evidencia/tuple-inline-view-v2.log`):

- `live`, `parameter_live`, `global_live`, `branch_live`, `parameter_scope`,
  `global_scope` y `multiple_live` compilaron y ejecutaron con salida 42 en O0 y O2.
- `escape`, `replace` y `multiple_escape` se rechazaron con el diagnóstico
  esperado durante análisis; no se ejecutaron.

La copia del fixture de esa ejecución está en
`A:/nsc-vistas-evidencia/tuple-inline-view-fixture-v2.py` y tiene SHA-256
`40931278c297f9c9d841378b7974ed576f7219e5613365fb36353bbed7016d7c`. Los ensayos
del reenvío genérico con productor fuente y binario aceptaron el control vivo y
rechazaron escape y reemplazo; sus logs son `generic-local-view-combined-v2.log`
y `named-view-generic-combined-v2.log` en la carpeta de evidencia. Esos ensayos
son semánticos; no ejecutaron consumidores ni acreditan el caso con Core externo
cuyo productor todavía fallaba al emitir metadata.

Un sondeo semántico de un constructor con dos argumentos `ref` aceptó el caso
con ambos dueños vivos y rechazó por su nombre tanto el escape de `first` como
el de `second`. El constructor solo conserva `second`, por lo que el rechazo de
`first` muestra el alcance conservador declarado arriba. Fuente, comandos y
diagnósticos están en `A:/nsc-vistas-evidencia/view-two-roots-u0f4ygtn/results.json`.
No se ejecutaron esos programas.

El sondeo separado `view-input-roots-xnedfjwi/results.json` aceptó el control
`ref` vivo y rechazó su escape. Sus dos variantes `in` fallaron antes de poder
evaluar lifetime, porque el constructor intentó asignar una dirección de solo
lectura a un puntero mutable. Por tanto, no constituyen evidencia de corrección
ni de defecto para raíces `in`.

El análisis dirigido `clang-tidy` con `clang-analyzer-core.*` terminó con código
0 para ambos archivos, con el borrador independiente de FnOnce presente.
`tuple-inline-assignment-tidy.log` no contiene diagnósticos;
`tuple-inline-ownership-tidy.log` contiene un aviso `CallAndMessage` en la línea
360. La traza supone que `expression` es nula, lo cual deja vacío el vector de
trabajo, y a la vez entra al ciclo que exige que no esté vacío. También supone
que `current` es nulo después del guard que lo descarta. Por estas
contradicciones de flujo, ese aviso concreto se clasifica como falso positivo;
no se modificó código ni se suprimió el diagnóstico. La región señalada no forma
parte del diff de esta corrección. Los logs están en `A:/nsc-vistas-evidencia/`.
Esto no certifica los demás análisis ni todo el repositorio.

La revisión estática posterior no encontró en estos controles una regresión
concreta del hunk, pero no sustituye pruebas de los demás casos ni verificación
de todas las formas de origen. El build contenía
otros cambios concurrentes y no certifica un commit aislado. La campaña sigue
abierta.

**Shadowing pendiente:** una vista que ya presta del `owner` exterior se
rechaza al asignarla en un ámbito con otro `owner`, aunque el original siga
vivo (`A:/nsc-vistas-evidencia/view-shadow-wbyo7np4/results.json`). Un contraste
con el compilador anterior a este ajuste produjo el mismo falso rechazo
(`view-shadow-k0tnhzj4/results.json`); no se atribuye al diferencial aquí
documentado. La representación actual guarda nombres en `viewSourceFields`,
por lo que este caso necesita procedencia con identidad semántica estable y
propagación entre scopes. No se considera resuelto por esta corrección.
