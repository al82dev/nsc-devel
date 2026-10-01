# Tanda operativa 41 — ownership, borrow, temporales y `foreach`

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Cerrar las limitaciones §20, §21, §24, §65, §66, §81, §94, §96, §107 y §144 sin
convertir aliases en owners ni relajar los escapes:

- mover un field `owned` a un owner local y destruir el objeto exactamente una vez;
- expresar `raii borrowed T[]`: el local posee el buffer, pero sus slots sólo prestan objetos;
- permitir `raii string += value` con reemplazo seguro del buffer anterior;
- mantener un owner RAII con nombre al desazucarar `foreach`, tanto para arrays como colecciones;
- reasignar locales y parámetros `borrowed` conservando todos sus posibles owner roots;
- prestar strings owned temporales durante una expresión completa a métodos, constructores,
  interfaces y funciones libres;
- declarar `String.ToCharArray` como productor `owned char[]`;
- aceptar una vista `borrowed` explícita desde un owner RAII y `null` como valor vacío para un
  parámetro `owned`.

## Hallazgos antes del cambio

Varias piezas ya existían, pero no componían un contrato completo:

- el move desde fields owned y el borrow explícito básico ya tenían soporte y se conservaron con
  ejecución, en lugar de reimplementarlos;
- el préstamo de string temporal funcionaba en métodos y constructores, pero las funciones libres
  no registraban el owner oculto de expresión completa;
- `foreach` protegía una expresión que devolvía un owner nuevo, pero copiaba un owner RAII ya
  nombrado hacia `$__foreach_source`;
- un array RAII destruía siempre sus elementos reference-like; no podía representar la combinación
  «buffer owned, elementos borrowed»;
- una local borrowed era artificialmente inmutable después de inicializarse, porque el modelo sólo
  guardaba una raíz de procedencia;
- `String.ToCharArray` construía memoria nueva pero su firma no publicaba `owned`;
- `classifyOwnershipArgument` exigía una transferencia aun para `null`, que no posee nada y es el
  estado vacío legítimo de un slot owned;
- el `+=` de string caía en el camino numérico de `CompoundStore`, incompatible con el cleanup de
  un owner vivo.

## Diseño e implementación

### Arrays con buffer owned y elementos borrowed

`raii borrowed T[]` tiene dos ejes distintos:

- `raii` crea el cleanup léxico del buffer nativo;
- `borrowed` describe los slots: leer/escribir un elemento no transfiere ownership del objeto.

El frontend admite esta combinación únicamente para arrays nativos. El identificador del array se
clasifica como borrowed al indexarlo, de forma que una asignación `aliases[i] = owner` guarda un
alias y no exige `move`. El cleanup LLVM recibe explícitamente si debe destruir elementos; para
este caso libera sólo el buffer y deja que los owners originales destruyan sus objetos.

### Reasignación borrowed con procedencia conservadora

Una binding borrowed conserva ahora conjuntos de posibles owner locals y owner parameters además
de la raíz original. Cada reasignación sólo admite una fuente nombrada y rastreable: owner RAII,
parámetro owned/borrowed u otra local borrowed.

Las raíces se acumulan, no se sustituyen. Esto es intencional para control flow: si dos ramas
reapuntan una vista a owners distintos, mover cualquiera se rechaza mientras la vista siga viva.
También se verifica que una local fuente no tenga un scope menor que el destino. Los parámetros
borrowed reasignados llevan un registro equivalente; un retorno que acabaría apuntando a una local
o a un parámetro owned sigue rechazándose.

### Reemplazo RAII de strings

`text += suffix` sobre `raii string text` produce un `StringConcat` owned. HIR materializa primero
el nuevo buffer, después ejecuta el cleanup protegido del valor anterior y finalmente guarda el
nuevo owner en el mismo slot. El orden evita tanto use-after-free como leak si el destino y un
operando se relacionan. No se extendió `+=` a fields/properties owned ni a destinos con efectos: la
forma admitida es una local RAII identificable.

### Temporales y funciones libres

La resolución de funciones de namespace registra los mismos argumentos de préstamo de expresión
completa que la resolución de métodos. Un string fresh-owned enviado a `borrowed string` —o a la
superficie legacy no-retaining de string— obtiene owner oculto y cleanup después de la llamada.
Esto cubre llamada libre e interfaz sin mantener una allow-list específica de Core.

### `foreach` sobre owners nombrados

El probe semántico marca la fuente sintetizada como borrowed cuando la expresión fuente es un
owner o borrow ya nombrado. El loop toma una vista temporal del array/colección y el owner original
permanece en su único slot. Las expresiones que devuelven un owner fresco conservan el camino RAII
existente.

### `null`, fields y `ToCharArray`

La clasificación de argumentos recibe de forma explícita si el argumento es `null`. Un parámetro
owned lo acepta sin fabricar un transfer binding; su cleanup ya es null-safe. La reproducción de
field move confirma por contador de destructor que el field queda vacío al moverlo y que el owner
receptor descarga una sola vez. `String.ToCharArray` declara `owned char[]`, alineando la firma con
la reserva que siempre realizó.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - amplía procedencia borrowed, agrega registro de parámetros reasignados y marca argumentos de
    préstamo temporal en candidatas de funciones libres.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - acepta `null` para owned, registra temporales en funciones libres, implementa procedencia
    acumulada, distingue arrays borrowed-element, conserva owners de `foreach` y valida string `+=`.
- `src/Frontend/ProjectConstructionModel.cpp`
  - pasa la clasificación explícita de `null` en constructores.
- `src/Frontend/ProjectOopHirModel.cpp`
  - baja `raii string +=` como concatenación owned seguida por reemplazo con cleanup.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - separa destrucción del buffer de destrucción de elementos para arrays nativos RAII.
- `lib/NativeSharp.Core/String.ns`
  - publica `ToCharArray` como `owned char[]`.
- `CMakeLists.txt`
  - registra exclusivamente `nsc.ns3.t41.ownership` cuando se habilitan pruebas.

## Archivos nuevos y razón

- `tests/integration/ns3_t41_ownership/T41Ownership.nsproj`
- `tests/integration/ns3_t41_ownership/Main.ns`
  - ejecutable O0 que observa destrucción exacta, aliases de array, rebind borrowed, string `+=`,
    temporales free/interface, `ToCharArray`, List foreach y owned-null.
- `tests/integration/ns3_t41_ownership/negative/ReturnReassignedLocal/*`
  - impide devolver una vista cuya procedencia acumulada incluye una local.
- `tests/integration/ns3_t41_ownership/negative/MoveReassignedOwner/*`
  - impide mover cualquiera de los owners posibles mientras el borrow está vivo.
- `tests/integration/ns3_t41_ownership/negative/ShortLivedRebind/*`
  - impide reapuntar un destino exterior hacia un owner de bloque interior.
- `cmake/VerifyNativeSharpT41Ownership.cmake`
  - compila/ejecuta sólo el positivo T41 y verifica los tres diagnósticos negativos.
- `docs/3.0/TANDA-OPERATIVA-41-OWNERSHIP-BORROW-TEMPORALES-FOREACH.md`
  - este expediente de diseño, cambios, archivos y evidencia.

## Validación dirigida

- Construcción: `cmake --build A:\nsc-devel\build-clang --config Debug --target nsc -j 12`.
- Verificador único: `VerifyNativeSharpT41Ownership.cmake`.
- Positivo: compilación O0 y ejecución física con exit `0`.
- Negativos:
  - retorno de borrow reenraizado en una local;
  - move del segundo owner después de reasignar la vista;
  - owner de scope interior asignado a una vista de scope exterior.

No se ejecutó CTest completo, baseline, O1/O2/O3 ni trabajo remoto; no se instaló ni modificó
`C:\native-sharp`.

## Mejora obtenida

Ownership deja de asumir que «poseer un array» implica «poseer cada referencia guardada» y que un
borrow sólo puede tener una raíz inmutable. Las operaciones cotidianas conservan una sola fuente de
destrucción, los temporales sobreviven exactamente hasta el final de su llamada, y `foreach` ya no
duplica owners al introducir su fuente oculta. Los rechazos siguen siendo fail-closed cuando el
alcance o la procedencia no pueden demostrarse.
