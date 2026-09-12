# 243 — Slots crudos genéricos / Generic raw slots

**Línea / Line:** `1.0.1-dev`

**Alcance / Scope:** monomorfización, análisis de ownership y consultas de layout en O0

## Resumen

Esta corrección completa el contrato de un almacenamiento genérico de una sola ranura:

```nativesharp
public class RawSlot<T>
{
    private T* storage;
    public RawSlot() { storage = new T; }
    public void Put(owned T value) { storage[0] = move(value); }
    public owned T Take() { return move(storage[0]); }
}
```

La misma plantilla ahora puede cerrarse con un tipo valor o con una referencia. `new T` conserva
su intención de reservar una ranura cruda después de sustituir `T`; un elemento de `T*` puede ser
destino y fuente explícita de `move`; y `sizeof(T)`/`alignof(T)` se convierten en consultas del
tipo concreto durante la clonación de la especialización.

## Causas corregidas

La prueba acumulativa exponía tres pérdidas distintas de información:

1. al sustituir `T` por una clase, `new T` terminaba pareciendo el source form diferente
   `new Clase`, que debe usar constructor;
2. el análisis de `move` aceptaba elementos de arreglos nativos y fijos, aunque el HIR ya sabía
   obtener exactamente la misma dirección para `p[i]`;
3. el parser conserva `sizeof(T)` y `alignof(T)` como expresiones cuando el parámetro genérico no
   está en su catálogo global de nombres de tipo; la clonación no convertía ese identificador en
   la consulta tipada de la especialización.

No se relaja la construcción ordinaria de clases ni se admite `move` sobre indexadores. Sólo se
reconoce la forma cruda y directamente direccionable que ya tiene lowering propio.

## Implementación

- `ProjectGenericModel::cloneNode` marca el `new T` escalar antes de sustituir el parámetro. La
  marca vive en el clon, no en la identidad del tipo y no altera `new T[count]` ni `new T(...)`.
- La misma clonación transforma consultas de layout cuyo operando es el parámetro sustituido a
  `SizeofTypeExpression`/`AlignofTypeExpression` con el `TypeSyntax` concreto y su origen.
- `ProjectMethodBodyModel` incluye raw pointers en el conjunto de elementos direccionables que
  pueden participar en una transferencia explícita. La comprobación negativa sigue excluyendo
  propiedades e indexadores semánticos.
- La asignación a `storage[i]` no adquiere cleanup implícito de arreglo poseído: `T*` permanece en
  el nivel crudo. `move` carga y pone a null únicamente una referencia, mientras que para valores
  es una lectura sin destrucción ficticia.

## Archivos

- `src/Frontend/ProjectGenericModel.cpp`: preserva la intención de `new T` y especializa las
  consultas de layout sobre parámetros genéricos.
- `src/Frontend/ProjectMethodBodyModel.cpp`: admite `move(p[i])` y `p[i] = move(value)` para raw
  pointers, manteniendo el diagnóstico específico.
- `tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp`: alinea la negativa contra
  indexadores con el contrato ampliado.
- `mkdown/243_1_0_1_slots_crudos_genericos.md`: historial y decisiones de esta entrega.

## Evidencia dirigida

`nsc.stage52.generic-collections-compiler-fixes` cubre en una sola plantilla `RawSlot<int>` y
`RawSlot<Payload>`, verifica tamaños/alineamientos 4 y 8, baja a LLVM, verifica el módulo y emite
COFF. La prueba separada de indexador continúa roja por diseño. No se ejecuta baseline en esta
entrega.

---

## English

This correction completes the one-slot generic raw-storage contract. A scalar `new T` retains
its raw-allocation intent after T is replaced by a class argument; direct raw-pointer elements
can be explicit move sources and destinations; and `sizeof(T)`/`alignof(T)` become typed layout
queries for the concrete specialization.

The change does not relax ordinary class construction and does not turn semantic indexers into
ownership sources. It only admits `p[i]`, whose raw address lowering already exists. Raw pointer
storage also remains unmanaged: no container cleanup is invented. Moving a reference loads and
nulls its slot, while moving a value is an ordinary value load.

`ProjectGenericModel::cloneNode` preserves the scalar generic raw-new source form and rewrites
layout queries over substituted parameters. `ProjectMethodBodyModel` extends the existing
native/fixed-array move checks to raw pointers. The directed Stage 52 contract closes the same
template over `int` and `Payload`, checks their canonical layouts, verifies LLVM, emits COFF, and
retains the negative custom-indexer guard. No baseline is run for this delivery.

## Commit de referencia / Reference commit

**Título ES:** Completa los slots crudos genéricos

**Title EN:** Complete generic raw slots

**Descripción ES:** Conserva `new T`, especializa `sizeof(T)`/`alignof(T)` y habilita transferencias
explícitas sobre elementos de `T*` sin ampliar ownership a indexadores.

**Description EN:** Preserve `new T`, specialize `sizeof(T)`/`alignof(T)`, and enable explicit
ownership transfers through `T*` elements without extending ownership to indexers.
