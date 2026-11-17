# Llamadas que preservan almacenamiento mientras vive una vista

## Problema

La regla de `llamadas-campo-prestado.md` rechaza toda llamada de instancia no
`readonly` (los métodos de clase siempre) y todo argumento de referencia mientras
vive una vista cuya raíz se solapa con el receptor o el argumento. El resultado de
una llamada recibe la raíz `G`, que se solapa con todo. Al compilar las bibliotecas
con nsc en O0 (`A:\nsc-devel-fixes-evidencia\libs-antes-O0.log`), esto rechazaba
llamadas a métodos de clase que sólo leen. Caso concreto:
`NativeSharp.Graph/src/Ordering/TopologicalSort.ns:96`, donde `g.NodeAt(target)`
se llama mientras vive el `EdgeCursor cursor` obtenido de `g`. `NodeAt` sólo lee
campos escalares y construye un `NodeId`.

## Causa

Para un método de clase no existe un contrato de solo lectura comprobado. La regla
original lo documenta como aproximación conservadora: "puede rechazar un método que
no modifica el campo". Ningún análisis intentaba demostrar lo contrario.

## Cambio

Inferencia diferida y sólida de callees que preservan almacenamiento
(`src/Frontend/Bodies/StorageViewCallInference.cpp`):

1. En los dos sitios de la regla (`validateSelectedCallReceiver` y la comprobación de
   argumentos de `completeSelectedCall`, en `CallAnalyzer.cpp`), si el callee es
   candidato, el diagnóstico se difiere en `deferredStorageViewCalls_` en lugar de
   emitirse. Un callee es candidato si es de fuente, no virtual, no override, no
   abstracto, no extern ni importado, no async ni generador, y es un método o
   constructor con cuerpo. En el sitio de receptor no se difiere un método de
   instancia de struct: su contrato sigue siendo `readonly`, verificado por el
   compilador (`tests/integration/view_readonly_members.py`).
2. Los shards fusionan esa lista (`ProjectCallableResultMerger::mergeShardState`).
3. Después de analizar todos los cuerpos (`MethodBodyBuildCoordinator`, tras
   `validateRequiredInliningCycles`), `finalizeStorageViewCalls` calcula el
   **punto fijo mayor** con dos worklists, sin recursión. La exploración revisa cada
   callee una vez y guarda aristas inversas; luego el rechazo se propaga hacia atrás
   a los dependientes. Un ciclo sin miembro rechazado sigue preservando. Se emite el
   diagnóstico original, con el mismo texto y la misma posición, para cada sitio
   cuyo callee quedó rechazado.

Un callee preserva almacenamiento cuando su cuerpo, recorrido con lista blanca sobre
sus propios bindings, cumple todo lo siguiente:

- su cuerpo fue analizado en este build (`callableBodyAnalyzed`);
- no tiene resultado ni parámetros `owned` o con drop de valor;
- no tiene locales estáticos, `raii`, `scope`, `owned` o con drop;
- sólo contiene estos tipos de nodo: bloques, declaraciones, `if`, `while`,
  `do`, `for`, `switch`, `break`, `continue`, `return`, expresiones, literales,
  `this`, operadores, condicional, asignación, llamada, índice, miembro, `->`,
  postfijo, cast, `sizeof`/`alignof`, `default` y creación de objeto. Por lo tanto
  quedan fuera `delete`, `destroy`, colocación, lambdas, ensamblador, `using`,
  `foreach`, `move`, `try`, `throw`, `await` y el resto;
- toda llamada, incluidos getters y operadores con binding, está resuelta, sin
  argumentos por defecto ni conversiones de usuario, y con firma que preserva. El
  callee de cada llamada debe ser a su vez candidato (dependencia del punto fijo) o
  un método de instancia `readonly` de struct (la confianza que la regla ya tenía).
  Un `CallExpression` sin binding (delegados, funciones de namespace) rechaza;
- no se usan eventos;
- una escritura (`=`, compuestas, `++`, `--`) a un local propio no referencia o a un
  parámetro por valor siempre es válida. Un constructor también puede inicializar
  sus propios campos. Cualquier otro destino (campo, elemento, desreferencia,
  parámetro `ref`/`out`, estático) debe tener un tipo cuyo valor anterior no libere
  nada: datos simples (`typeIsPlainData`) o puntero;
- una creación `new S(...)` es de un struct sin drop de valor, sin asignador ni
  inicializador. Su constructor elegido (`previewConstructionConstructor`) y su
  constructor estático, si existe, son dependencias del punto fijo;
- en `return`, inicializadores y asignaciones no hay conversión implícita de
  usuario posible (ninguno de los dos tipos declara `implicit`).

## Comportamiento conservado

- Un callee que puede liberar memoria se sigue rechazando con el mismo diagnóstico,
  directo, por cadena de llamadas y dentro de un ciclo.
- Los callees precompilados, virtuales, de interfaz, abstractos, extern y los
  delegados siguen rechazándose como antes (salvo los métodos `readonly` de struct,
  que la regla ya aceptaba).
- Los métodos de instancia de struct no `readonly` se siguen rechazando.
- Un programa sin vistas vivas no difiere nada: la lista queda vacía y
  `finalizeStorageViewCalls` regresa de inmediato.

## Límites conocidos

- Los inicializadores de campos estáticos (inicialización perezosa del tipo) no se
  inspeccionan; sólo el constructor estático del tipo creado.
- Un callee precompilado no se puede demostrar: en `NativeSharp.Net` el constructor
  de `ReadOnlySpan<byte>` lo exporta Core ya compilado, por lo que `HeaderValue` y
  `HttpRequestHeaders.Value` no se demuestran. Esos dos sitios se resolvieron en la
  biblioteca (LIB-422). Demostrarlos requeriría publicar el resumen en NSMETA,
  que es un cambio de metadata y no se hizo.

## Pruebas

- Nueva: `tests/integration/storage_preserving_callee.py`.
- Regresión dirigida sin modificar las pruebas: ver la sección de evidencia.

## Evidencia

En `A:\nsc-devel-fixes-evidencia`, con el nsc de este árbol (`compilar-nsc-4.log`, sin
errores ni warnings):

- `nuevas-final\storage_preserving_callee.log`: los seis casos válidos salen con 42 en
  O0, O1, O2 y O3; los cuatro casos que reemplazan el campo (directo, por cadena, en
  ciclo y como argumento) se rechazan.
- Regresión dirigida sin modificar las pruebas (`regresion.cmd`, logs en
  `regresion-final\`): `shared_field_method_borrow`, `shared_binary_pattern_field`,
  `binary_view_owner_field`, `binary_view_owner_field_in`, `binary_view_receiver_output`,
  `borrowed_bound_delegate_alias`, `borrowed_bound_delegate_output`,
  `borrowed_view_owner_field`, `borrowed_view_receiver_output`, `view_readonly_members`,
  `operator_increment_owned_field`, `readonly_pattern_loan`, `borrowed_lambda_owned_view`,
  `library_antecedent_boundaries` y `shared_public_pointers` terminan con 0.
  `nsc_stage58_view_lifetimes_tests` da `PASS: 71 checks` (`stage58-paso2b.log`).
- Un primer ensayo difería también los métodos de instancia de struct. Rompía el caso
  "non-readonly method with a live view" de `view_readonly_members.py`
  (`regresion-paso2\view_readonly_members.log`), por eso se excluyen en el sitio de
  receptor.
- Bibliotecas (`libs.cmd paso2`): 31 de 31 paquetes en O0 y en O2
  (`libs-paso2-O0.log`, `libs-paso2-O2.log`), junto con las correcciones de biblioteca
  LIB-419 a LIB-424. Sitio que resuelve directamente esta inferencia:
  `TopologicalSort.ns:96` (`g.NodeAt`). Sonda: `temp\sonda`.

No verificado: la batería completa de ctest (la configuración apunta su compilador a
`C:\native-sharp\bin\nsc.exe`), ni O1/O3 en las bibliotecas.
