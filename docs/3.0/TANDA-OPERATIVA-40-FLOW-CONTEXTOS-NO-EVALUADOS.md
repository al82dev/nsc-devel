# Tanda operativa 40 — flow, cortocircuito y contextos no evaluados

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Cerrar las limitaciones importadas §40/§115, §43/§56 y §135, y volver a verificar §13 sin
debilitar la asignación definitiva:

- conservar por separado el estado de las aristas verdadera y falsa de `&&`, `||` y `!`;
- usar esas aristas en `if` y `?:`, especialmente cuando un brazo termina con `return`, `throw` o
  una expresión `never`;
- permitir consultar el tipo de una local sin asignar mediante `sizeof`/`alignof`, sin convertir
  los efectos escritos dentro del operando en efectos reales;
- mantener explícito que el resultado de `sizeof` es `nuint`;
- no fingir que cualquier función que recibe `T*` escribe el pointee: exigir `out` o una
  inicialización explícita y diagnosticar el motivo.

## Reproducción anterior al cambio

El fixture se ejecutó primero con el compilador generado por T39. Falló con cuatro falsos positivos:

```text
use of unassigned local variable 'first'
use of unassigned local variable 'second'
use of unassigned local variable 'third'
use of unassigned local variable 'uninitialized'
```

`first` y `second` eran llenados por dos llamadas `out` en una cadena `||`; alcanzar el código
posterior demostraba que ambas llamadas se ejecutaron. `third` era leído dentro del brazo verdadero
de `Gate() && SetInt(out third, ...)`, donde la evaluación del RHS también era necesaria.

`sizeof(uninitialized)` medía un `int` pero el análisis de su identifier se ejecutaba como una
lectura antes de restaurar el snapshot de flow. La restauración existente evitaba que
`sizeof(value = 42)` marcara `value` como asignado, pero no suprimía el diagnóstico producido al
consultar una local sin valor.

El caso pointer `Store(&value)` se rechazaba, correctamente en lo conservador, pero sólo decía
`use of unassigned local variable`. No explicaba que una firma `int*` no promete escribir ni que el
contrato expresable para ello es `out int`.

§13 ya estaba corregida por T29. Se conservó una reproducción de Euclides extendido con dos `out`,
múltiples niveles recursivos y verificación `p*x + q*y == gcd`; no requirió otro cambio de ABI.

## Diseño e implementación

### Estados true/false por expresión booleana

`OwnershipFlowSnapshot` ya contenía moves, borrows, alcance y el conjunto MUST de locales asignadas.
El defecto era perder información al colapsar inmediatamente las dos salidas de una expresión lógica.

T40 agrega un mapa privado por nodo de sintaxis con dos snapshots:

- `whenTrue`: estado si el valor de la expresión es verdadero;
- `whenFalse`: estado si el valor es falso.

La composición respeta el CFG real:

- `A && B`: true sale de `B.true`; false une `A.false` con `B.false`;
- `A || B`: true une `A.true` con `B.true`; false sale de `B.false`;
- `!A`: intercambia las dos aristas.

El RHS parte de `A.true` para `&&` y de `A.false` para `||`; por tanto, las cadenas anidadas y los
paréntesis no necesitan casos especiales. El estado general posterior continúa siendo el join de
ambas aristas, de modo que una expresión almacenada en un `bool` no atribuye una asignación que pudo
omitirse.

`if` analiza su cuerpo verdadero desde `condition.true` y el falso desde `condition.false`. El join
existente ya ignora una salida `reachable=false`, por lo que `return`, `throw`, `panic()` y cualquier
otro terminador reconocido dejan vivo exactamente el estado del camino que continúa. `?:` usa el
mismo par de entradas para sus ramas.

### `sizeof` y `alignof` realmente no evaluados

Durante el análisis del operando se activa un contexto no evaluado que suprime solamente la lectura
de asignación definitiva. Nombres, tipos, miembros y errores semánticos continúan resolviéndose.
Después se restaura el snapshot completo, como antes, para que asignaciones, moves y borrows escritos
dentro del operando no modifiquen el programa.

Esto permite:

```nativesharp
int value;
nuint bytes = sizeof(value);
```

pero sigue rechazando:

```nativesharp
int value;
nuint bytes = sizeof(value = 42);
return value; // la asignación nunca ocurrió
```

No se añadió conversión implícita desde `nuint` a `int`; la política segura existente permanece.

### Escrituras mediante raw pointer

Una firma `void Store(int* p)` no expresa si lee, escribe condicionalmente, conserva o ignora el
pointee. T40 no inventa resumen interprocedural ni marca la local como asignada. Al formar `&value`
sobre una local sin asignar se emite un único diagnóstico causal:

```text
taking the address of unassigned local 'value' through a raw pointer does not establish definite
assignment; declare the callee parameter as 'out int' or initialize the local explicitly
```

Una firma real `out int` continúa asignando en toda salida normal y participa en las aristas
booleanas nuevas.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - añade los estados condicionales privados y la marca de contexto no evaluado.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - compone aristas true/false para `&&`, `||` y `!`;
  - alimenta `if` y `?:` desde la arista correcta;
  - suprime lecturas de definite assignment dentro de `sizeof`/`alignof` sin conservar efectos;
  - emite el contrato causal para dirección de una local no asignada.
- `CMakeLists.txt`
  - registra sólo `nsc.ns3.t40.flow` cuando las pruebas están habilitadas.

## Archivos nuevos y razón

- `tests/integration/ns3_t40_flow/T40Flow.nsproj`
- `tests/integration/ns3_t40_flow/Main.ns`
  - ejecutable O0 con `||`, `&&`, terminación `panic`, tres clases de `out`, recursión y `sizeof`.
- `tests/integration/ns3_t40_flow/negative/SkippedOut/*`
  - prueba que un RHS omitible no asigna la local.
- `tests/integration/ns3_t40_flow/negative/SizeofDoesNotAssign/*`
  - prueba que una asignación escrita dentro de `sizeof` no sale del contexto no evaluado.
- `tests/integration/ns3_t40_flow/negative/RawPointerWrite/*`
  - congela el rechazo causal de un supuesto contrato de escritura mediante `T*`.
- `cmake/VerifyNativeSharpT40Flow.cmake`
  - compila/ejecuta sólo el positivo T40 y verifica los tres diagnósticos negativos.
- `docs/3.0/TANDA-OPERATIVA-40-FLOW-CONTEXTOS-NO-EVALUADOS.md`
  - este expediente de reproducción, diseño, archivos y evidencia.

## Validación dirigida

- Construcción: `cmake --build A:\nsc-devel\build-clang --config Debug --target nsc -j 12`.
- Verificador único: `VerifyNativeSharpT40Flow.cmake`.
- Positivo: compilación O0 y ejecución física con exit `0`.
- Negativos:
  - `SkippedOut`: lectura de local no asignada;
  - `SizeofDoesNotAssign`: la asignación no escapa de `sizeof`;
  - `RawPointerWrite`: recomendación explícita de `out int` o inicialización.

No se ejecutó CTest completo, baseline, O1/O2/O3 ni trabajo remoto; no se instaló ni modificó
`C:\native-sharp`.

## Mejora obtenida

La asignación definitiva deja de ser sólo un snapshot después de una expresión y pasa a conservar
la información que realmente viaja por cada arista booleana. Esto elimina inicializadores falsos en
validaciones idiomáticas sin aceptar lecturas inseguras. Los contextos no evaluados consultan tipos,
no valores, y el raw pointer deja de parecer una promesa de escritura que su firma nunca hizo.
