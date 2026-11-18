# Hijo `owned` creado en el constructor de un objeto de clase local

Rama `development-main-fixes`. Campaña: mejoras de `X:\Examples-NSC\Problemas\mejoras.md`
(construcción local de objetos de clase). Evidencia: `A:\nsc-main-fixes-evidencia\claude\owned-child-run`
y `A:\nsc-main-fixes-evidencia\claude\CAUSA-owned-child.txt`.

## SCOPE LOCK

- Objetivo: que `Container value;` se construya en el marco cuando el constructor ejecutado crea un
  hijo real en un campo `owned` de clase inicializado a `null`, sin relajar ninguna otra prueba.
- Naturaleza: funcional (capacidad del lenguaje en curso de implementación en esta campaña).
- Debe permanecer idéntico: todo programa que no declare objetos de clase locales implícitos;
  rechazos de publicación del receptor; constructores con argumentos, `throws`, `try`, otros
  inicializadores de campo; el caso `owned Child child = null` sin asignación (commit d224d1421).
- Base de la campaña: 152654e5a0e2d84d4c2f245ea7590488f0497430; revisión de partida: d224d1421.
- Archivos modificables: `src/Frontend/Bodies/ClassReceiverEffectAnalysis.cpp`,
  `src/Frontend/ProjectConstructionModel.cpp`, `include/nativesharp/Frontend/ProjectConstructionModel.h`,
  `tests/unit/ClassValueOriginsTests.cpp`, este documento.
- Protegidos: ejemplos originales, `C:\native-sharp`, `A:\nsc-limitaciones-vistas`, interoperabilidad C/C++,
  batería histórica.
- Prohibido: merge, rebase, push, despliegue, batería completa, reconstrucción completa repetida.
- Terminación de este cambio: reproducción aceptada, controles negativos rechazados, ejecución O0/O2
  con conteo exacto de construcciones y destrucciones.

## Causa

`OwnedNullCtor.nsproj` (`owned Child child = null; public Container() { child = new Child(); }`) se
rechazaba con `local class construction or destruction is not proven safe`. El resumen de efectos del
receptor del constructor (`finalizeClassReceiverEffects`) solo aceptaba asignaciones `this.campo = literal
escalar | parámetro escalar` y escrituras estáticas escalares; `child = new Child()` dejaba el resumen
en `unknown`, y `assessClassConstructorReceiver` devolvía `Unknown`.

## Contrato implementado

Una sentencia de constructor de instancia se acepta como libre de escape del receptor solo si tiene
exactamente la forma `campo = new T();` o `this.campo = new T();` y:

1. el campo es de instancia, del mismo tipo que declara el constructor, `owned`, de clase, con
   inicializador `null` (`isClassLocalNullOwnedClassField`);
2. `new T()` no tiene argumentos, inicializador ni sintaxis adicional, y su tipo es exactamente el tipo
   declarado del campo (sin conversión a base);
3. la asignación no tiene operador de usuario (`callBinding == nullptr`).

El receptor solo se usa como dirección base de su propia ranura; el hijo no recibe ningún valor, por lo
que no puede observar el objeto del marco.

El modelo de construcción añade una segunda condición, por constructor
(`markUnprovenConstructorChildren`, `classLocalChildConstructionIsClosed`): toda construcción de
objeto dentro del constructor que el objeto local ejecuta debe estar resuelta, sin argumentos,
predeterminados, conversiones, paquetes, transferencias ni asignador explícito; el constructor elegido
del hijo no puede declarar error, tener efecto `Throws` ni suspenderse; y ningún destructor de la
cadena de tipos del hijo puede declarar error, tener efecto `Throws` ni suspenderse. La marca se
consulta solo para el constructor ejecutado (rama sin base y cadena de bases), de modo que una
sobrecarga no usada conserva el comportamiento previo.

## Semántica de ausencia

Un programa sin declaraciones de clase local implícitas no ejecuta ninguna de las rutas nuevas. Un tipo
cuyos constructores no contienen `new` no recibe marcas. Una sobrecarga que construye objetos no
cerrados solo afecta a los objetos locales que la ejecutan.

## Ownership y lifetime observados (IR O0 de `OwnedChildRun`)

- `Run`: `alloca [16 x i8]` para el contenedor, `memset`, descriptor y llamada directa al constructor.
- Constructor: `__nativesharp_alloc(8)` para el hijo, guarda del valor anterior del campo (destruye y
  libera un valor previo no nulo) y almacenamiento del nuevo hijo.
- Salidas: retorno temprano, retorno normal y `cleanuppad` del `invoke` de `Read` llaman a la entrada
  7 del descriptor (destrucción sin liberar el marco). `__nsc_destroy_object_..Container` ejecuta el
  cuerpo del destructor y luego destruye el campo `owned` con guarda de null usando la entrada 5
  (destruir y liberar el hijo del heap).

## Pruebas

- `nsc.class-value-origins` (`tests/unit/ClassValueOriginsTests.cpp`):
  - positivos nuevos: campo implícito, campo con `this.` junto a escritura escalar y destructor del hijo,
    constructor base que crea el hijo con objeto local derivado, sobrecarga ajena que publica `this`;
  - negativos nuevos: hijo que recibe `this`, publicación posterior a la creación, valor de tipo
    derivado, argumento escalar, constructor del hijo con `throws` atrapado con `try`, método que
    reemplaza el hijo y método que lo mueve fuera.
  - Cambio de expectativa: la entrada negativa `LocalOwnedNullConstructorAssignment`, agregada en esta
    misma campaña (d224d1421) como marcador de "aún no demostrado", se sustituye por el positivo
    `LocalOwnedChildImplicitField` con el mismo programa. No es una prueba de la batería histórica.
- Ejecución O0/O2 de `claude\owned-child-run`: retorno temprano y normal, objeto derivado; exige 3
  hijos creados, 3 hijos destruidos, 3 destructores del contenedor y 1 del derivado (`PASS O0 O2`).

## Límites (no verificado / pendiente)

- Hijo de tipo derivado del tipo del campo, argumentos en `new`, inicializador de campo
  `owned Child child = new Child()`, campos `owned` sin inicializador, arreglos `owned`, `raii` en
  campos y constructores con `try`, ramas o llamadas siguen fuera de esta prueba y se rechazan.
- Métodos que leen, reemplazan, mueven o destruyen el hijo después de construir siguen rechazados.
- Falla de asignación del hijo: el IR guarda `null` en el campo, igual que la versión en heap; no se
  ejecutó esa ruta.
- No se ejecutó la batería completa.
