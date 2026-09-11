# 1.0.1 — Constructores implícitos como definiciones ODR por símbolo

## Resumen

La compilación de una DLL que consume una biblioteca NativeSharp estática podía
llegar correctamente hasta el enlace y terminar con `NSL1021` porque el mismo
constructor implícito sintetizado por NSC aparecía como definición fuerte en dos
objetos. El primer caso observado fue la raíz del lenguaje:

```text
__nsc_ctor_NativeSharpFoundation::NativeSharp.Core.Object__Object
```

Una copia provenía del objeto del consumidor y otra de la biblioteca estática
precompilada. `nslink` se comportaba correctamente al rechazar dos definiciones
fuertes; el defecto estaba en la clasificación de linkage realizada por `nsc`.

## Causa

`ProjectOopHirModel` crea una función física para una clase fuente sin constructor
escrito cuando esa construcción debe publicarse a consumidores. La función se
marca con `isSyntheticImplicitConstructor`, pero el backend sólo aplicaba
semántica ODR a entidades de especializaciones genéricas.

Por tanto, un constructor implícito no genérico se emitía con linkage externo
fuerte aunque:

- su nombre físico lo determina completamente NSC a partir de la identidad
  nominal del tipo;
- su cuerpo lo sintetiza NSC a partir del mismo contrato de construcción;
- el mismo tipo puede llegar al enlace final por una dependencia fuente y por un
  productor estático precompilado.

`NativeSharp.Core.Object()` fue el primero en colisionar porque es la raíz común
de las clases, pero la regla incorrecta afectaba a cualquier constructor
implícito sintetizado, no sólo a `Object`.

## Corrección

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

Se separó la operación general `applyOdrDefinition()` de la condición específica
para genéricos:

- asigna linkage LLVM `weak_odr`;
- obtiene el nombre físico real de la función o global;
- crea un COMDAT `SelectionAny` identificado por ese mismo símbolo;
- coloca la definición en ese COMDAT.

`applyGenericOdrDefinition()` conserva su contrato anterior y delega la parte
física en el helper general.

Durante `declareFunctions()`, NSC aplica ahora la misma definición ODR por símbolo
a toda función marcada como `isSyntheticImplicitConstructor`, aunque no sea
genérica. De ese modo, COFF conserva una sola copia equivalente al enlazar un
productor y un consumidor.

La corrección no convierte en débiles los constructores escritos. Un constructor
declarado por el programador continúa siendo una definición fuerte; si dos
implementaciones reales reclaman la misma identidad física, el linker todavía lo
diagnostica.

## Prueba de regresión añadida

### `tests/unit/ProjectOopLoweringTests.cpp`

El caso integral de lowering comprueba en el LLVM generado que:

- los constructores implícitos de `Demo.Empty` y `Demo.Factory` se emiten como
  `weak_odr`;
- cada definición usa el camino COMDAT por símbolo ya verificado por el backend;
- el constructor escrito de `Demo.Counter` no se convierte en `weak_odr`.

La prueba cubre la frontera importante: deduplicar exclusivamente las entidades
sintetizadas por el compilador sin silenciar conflictos entre cuerpos escritos.

## Archivos modificados

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - nuevo helper `applyOdrDefinition()`;
  - reutilización desde `applyGenericOdrDefinition()`;
  - clasificación ODR de constructores implícitos en `declareFunctions()`.
- `tests/unit/ProjectOopLoweringTests.cpp`
  - aserciones de linkage para constructores implícitos y escritos.
- `mkdown/205_1_0_1_constructores_implicitos_odr.md`
  - diagnóstico, diseño, alcance y referencia de commit.

## Alcance deliberado

No se modificó `nslink`: su diagnóstico de símbolos fuertes duplicados era
correcto. Tampoco se hizo débil toda función NativeSharp. La política se limita a
constructores implícitos cuya implementación y símbolo son propiedad de NSC, y a
las entidades genéricas que ya tenían contrato ODR.

No se ejecutó baseline ni se compilaron bindings o Saru. La validación operativa
de esta entrega es reconstruir e instalar únicamente el compilador, conforme al
flujo rápido vigente.

## Referencia de commit

### Título / Title

```text
Deduplica constructores implícitos entre módulos / Deduplicate implicit constructors across modules
```

### Descripción / Description

```text
ES: Emite los constructores implícitos sintetizados como definiciones weak_odr con COMDAT por símbolo, evitando que una dependencia fuente y una biblioteca precompilada definan dos veces Object(). Mantiene fuertes los constructores escritos y agrega cobertura de lowering para ambas políticas.

EN: Emit synthesized implicit constructors as weak_odr definitions with per-symbol COMDAT, preventing a source dependency and a precompiled library from defining Object() twice. Keep written constructors strong and add lowering coverage for both policies.
```

## English summary

NSC emitted compiler-synthesized implicit constructors as strong external
definitions unless they belonged to a generic specialization. The same nominal
type can legitimately be present through a source dependency and a precompiled
static producer, so the final link could see two strong copies of the generated
constructor. `NativeSharp.Core.Object()` exposed the defect first.

Synthesized implicit constructors now use `weak_odr` and a per-physical-symbol
COMDAT. Written constructors remain strong, preserving diagnostics for genuine
duplicate user implementations. The regression test verifies both sides of that
boundary.
