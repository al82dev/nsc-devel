# Congelamiento 3.0, elemento 130a — constructores de genéricos cerrados bajo demanda

Fecha: 2026-10-26. Revisión de partida: `6afc21151`.

## Nota de trabajo

```text
HEAD: 6afc21151
Elemento: 130a del plan de 146 (hallazgo F4, reproducido al verificar Dictionary y HashSet, 121 y 122)
Contrato vigente: NSC-LIMIT-018 (cuerpos de genéricos cerrados bajo demanda) y la decisión de Alberto
  (2026-10-26) que lo extiende a los constructores, abajo
Documento histórico que podría contradecirlo: el comentario de NSC-LIMIT-018 en MethodBodyBuildCoordinator.cpp
  («Constructors/destructors are lifetime machinery … remain eager»); se actualiza para los constructores
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyBuildOptions.h
  include/nativesharp/Frontend/ProjectConstructionModel.h
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  src/Frontend/ConstructionModel/ConstructionBinding.cpp
  src/Frontend/ConstructionModel/ConstructorInitializers.cpp
  src/Frontend/ProjectConstructionModel.cpp
  src/Frontend/Bodies/MethodBodyBuildCoordinator.cpp
  src/Frontend/HIR/HirBuildCoordinator.cpp
  src/Frontend/ProjectCompilation.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

`Dictionary<K, V>` y `HashSet<K>` con una `K` clase o struct no compilaban ni con el constructor que recibe
`HashFunc<K>`:

```text
Dictionary.ns:68:22: error: no method in the method group exactly matches delegate signature 'HashFunc<Box>'
Dictionary.ns:74:22: error: ... (lo mismo)
```

`Dictionary()` y `Dictionary(int)` hacen `hasher = Hashing.Default`, que sólo existe para claves primitivas. Ningún
programa los elegía, pero se analizaban igual. Causa: NSC-LIMIT-018 difiere hasta su primera llamada sólo los
cuerpos de los métodos ordinarios de un tipo genérico cerrado. Los constructores se analizaban siempre, aunque
nada los eligiera. No viene de P1; el diseño es de la etapa 44. La sonda `A:\temp\nsc-dec\d30` lo aísla:

| Sonda | Forma | Antes |
|---|---|---|
| F4bBadUnusedMethod | método inválido para `Box`, sin usar | compila |
| F4aBadUnusedCtor | constructor inválido para `Box`, sin usar | rechazada |
| Z6ClassKeyNoDtor | `Dictionary<Key, int>(hf)`, `Key` clase sin destructor | rechazada |

La elección del constructor la hace `ProjectConstructionModel`, que se construye después del modelo de cuerpos. En
la biblioteca no hay arreglo posible: todo lo que el constructor por defecto toca se demanda.

## Decisión de Alberto (2026-10-26)

Propuesta mínima aprobada, con este alcance:

- Sólo constructores de especializaciones genéricas cerradas.
- Se analizan y bajan cuando una construcción los selecciona mediante `new`, `this(...)` o `base(...)`.
- Un constructor no seleccionado no puede invalidar la especialización.
- Si se selecciona el constructor por defecto, su dependencia de `Hashing.Default<K>` se diagnostica normalmente.
- Destructores, constructores estáticos, virtuales y tipos no genéricos permanecen ansiosos.
- La superficie pública de bibliotecas sigue sembrándose sin forzar cuerpos no demandados.
- El consumidor precompilado puede demandar y materializar el constructor elegido.

## Contrato

1. **Diferido.** Un constructor de instancia cuyo tipo es una especialización genérica cerrada
   (`stage44-specialization:`) entra a la lista de cuerpos sólo si se demanda (`isDeferredGenericMethod`). HIR
   no baja uno que no se analizó (`regularFunctionShouldBeSkipped`).
2. **Demanda.** Antes del modelo de cuerpos, `ProjectCompilation` crea un `ProjectConstructionModel` resolvedor
   (`attachDemandResolver`). Al cerrar cada ola, `collectConstructorDemands` le pregunta:
   - qué constructor elige cada sitio de construcción de los callables recién analizados
     (`previewConstructionConstructor`);
   - qué constructor elige el `this(...)`/`base(...)`/`base()` implícito de cada constructor recién analizado
     (`previewInitializerConstructor`).

   Las dos preguntas usan la misma evaluación y selección de candidatos que `build()`, sobre bindings de descarte
   y sin diagnósticos. Si el elegido está diferido, se encola, y así la cadena se sigue constructor por
   constructor.
3. **Inicializadores de campo.** Se analizan después de las olas; sus construcciones demandan en olas
   adicionales.
4. **Default implícito.** Una clase sin constructores explícitos encadena al constructor sin argumentos de la base
   más cercana que los tiene. Si ese constructor es de un genérico cerrado, se demanda de entrada
   (`seedImplicitChainConstructors`), porque el default implícito se enlaza para toda clase, se construya o no.
5. **Construcción.** `ProjectConstructionModel::build` no enlaza el inicializador de un constructor diferido no
   analizado. Si un binding elige uno que no se demandó, emite un error interno (`reportUndemandedConstructors`).
   Es un chequeo de consistencia, no una regla del lenguaje.
6. **Bibliotecas.** La superficie pública sigue sembrando los métodos; los constructores no se siembran. El
   consumidor instancia la plantilla desde NSMETA y demanda el constructor que elige.

## SCOPE LOCK

- **Cambia.** Sólo cuándo se analiza y baja un constructor de instancia de un genérico cerrado.
- **Idéntico.**
  - Tipos no genéricos.
  - Destructores, constructores estáticos y métodos virtuales.
  - La resolución de sobrecarga de constructores y sus diagnósticos, que siguen en `ProjectConstructionModel::build`.
  - El `where TKey : copy` de los constructores con `HashFunc` (P2): una clave struct move-only sigue rechazada,
    ahora con ese diagnóstico en lugar del de `Hashing.Default`.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. La base, antes del cambio, falló en K1, K4,
K8 y K9.

Sondas `A:\temp\nsc-dec\d34`, en `-O0` y `-O2`:

| Sonda | Forma | Resultado |
|---|---|---|
| K1UnusedBadCtor | `new Keeper<Box>(3)` con `Keeper()` inválido para `Box` | compila y corre |
| K2ChosenBadCtor | `new Keeper<Box>()` | rechazada en el cuerpo de `Keeper()` |
| K3IntDefault | `new Keeper<int>()` | corre |
| K4Chain | `new Chain<Box>(4)`: `this(a, 1)` → `base(a + b)` | 523 |
| K5ChainReachesBad | `new Chain<Box>()`: `base()` implícito llega a `Root()` | rechazada |
| K6ImplicitDerived | `class Leaf : Root<Box> { }` | rechazada (el default implícito elige `Root()`) |
| K7ImplicitDerivedInt | `class Leaf : Root<int> { }` construida | corre |
| K8Struct | `new Cell<Box>(5)` con `Cell(long)` inválido | corre |
| K9FieldInit | campo `owned Keeper<Box> K = new Keeper<Box>(6)` | corre |
| K10FieldInitBad | campo con `new Keeper<Box>()` | rechazada |
| K11DictDefault | `new Dictionary<Box, int>()` | rechazada sólo en `Dictionary()` (línea 68) |
| K12DictGuardKey | `Dictionary<Guard, int>(hf)` | rechazada por `where TKey : copy` |

Biblioteca precompilada (`A:\temp\nsc-dec\d35`): `LApi` con `Store<T>` (`Store()` inválido para `Box`) y
`Chain<T> : Store<T>` compila como biblioteca. El consumidor `new Store<Box>(3)`, `new Store<int>()`,
`new Chain<Box>(4)` da 3100; el consumidor con `new Store<Box>()` se rechaza.

Sondas d30 de colecciones: DictBoxKey (8 de 8), HashSetBox (14 de 14) y Z6ClassKeyNoDtor ahora compilan y
destruyen cada valor una vez. El resto no cambia; UnionWith/SymmetricExceptWith/GetViewBetween con elemento clase
siguen rechazados por C-01 (F5).

Sin cambios en el resto (`seq.cmd f130a`): `all.cmd` idéntico al de `f121`, `asynccases` 86, `gencases` 76 y
`loopcases` 10 PASS; los 66 programas de Freeze30 en `-O0` y `-O2`; los 27 proyectos de `tests/cases`
verifican; d28 y las tres bibliotecas como antes.

Pruebas nuevas, que compilan en `A:\temp\nsc-dec\bt` y se ejecutan en el gate final:

- `Freeze30ContractTests::testGenericConstructorDemand`:
  - F130aUnusedInvalidConstructor: 3;
  - F130aChosenInvalidConstructor: rechazada;
  - F130aChain: 523;
  - F130aChainReachesInvalid: rechazada;
  - F130aDictionaryClassKey: 22;
  - F130aHashSetClassKey: 33;
  - F130aDictionaryDefaultClassKey: rechazada.
- `testGenericConstructorDemandLibrary`: biblioteca y dos consumidores, 3100 y rechazo.

## Limitación que queda

El diagnóstico de un constructor elegido inválido sale en su cuerpo (la plantilla), no en el `new` que lo elige,
igual que para un método genérico demandado.

## No verificado

- Batería completa y unitarias: gate final.
