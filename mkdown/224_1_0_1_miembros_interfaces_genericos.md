# 224 — Miembros e interfaces genéricos / Generic members and interfaces

## Resultado / Outcome

**ES.** Las interfaces forman parte del sistema de plantillas monomorfizadas de NativeSharp. Una
declaración `interface I<T>` puede especializarse, heredarse, usarse como constraint, implementarse
por una clase genérica, cruzar NSMETA y participar en dispatch dinámico. El mismo escenario combina
un miembro estático de tipo cerrado (`Box<int>.Create`) y una plantilla restringida creada por el
consumidor.

**EN.** Interfaces are now first-class monomorphized templates. An `interface I<T>` can be closed,
inherited, used as a constraint, implemented by a generic class, transported through NSMETA, and
used for dynamic dispatch. The same contract combines a closed-type static member
(`Box<int>.Create`) with a constrained template first instantiated by the consumer.

## Superficie aceptada / Accepted surface

```nativesharp
public interface IValue<T>
{
    public T Get();
}

public interface ITaggedValue<T> : IValue<T>
{
    public int Tag();
}

public class Box<T> : ITaggedValue<T>
{
    private T stored;
    public T Get() { return stored; }
    public int Tag() { return 2; }
    public static Box<T> Create(T value) { return new Box<T>(value); }
}

public class Reader<T> where T : IValue<int>
{
    public static int Read(T value) { return value.Get(); }
}
```

El consumidor puede cerrar `ITaggedValue<int>`, convertir `Box<int>` a esa interfaz, invocar el
slot `Tag`, satisfacer `Reader<T>` con la interfaz base y llamar `Box<int>.Create` sin fuentes del
productor.

The consumer can close `ITaggedValue<int>`, convert `Box<int>` to that interface, invoke the `Tag`
slot, satisfy `Reader<T>` through the base interface, and call `Box<int>.Create` without producer
sources.

## Causa del hueco / Gap cause

`ProjectGenericModel::isGenericTypeTemplateNode` reconocía clases, structs y delegates, pero no
`InterfaceDeclaration`. El parser conservaba los parámetros de tipo, aunque la declaración nunca
obtenía un `GenericTemplateRecord`; posteriormente `IValue<int>` no podía resolverse como plantilla
visible. Era una omisión de clasificación, no una ausencia de infraestructura de interfaces.

`ProjectGenericModel::isGenericTypeTemplateNode` recognized classes, structs, and delegates, but
not `InterfaceDeclaration`. The parser preserved type parameters, yet the declaration never gained
a `GenericTemplateRecord`, so `IValue<int>` could not resolve as a visible template. This was a
classification omission rather than missing interface infrastructure.

## Implementación / Implementation

### Identidad de plantilla / Template identity

- Se agrega `GenericTemplateKind::Interface` al final del enum.
- Se conserva el valor numérico de `Class`, `Struct`, `Delegate` y `Method`, que ya se serializa en
  NSMETA 1.11/1.12.
- La clave sigue la regla nominal existente:
  `módulo::namespace.IValue` + aridad + argumentos canónicos.
- La cache reutiliza una sola especialización para cada interfaz cerrada.

`Interface` is appended to the enum so all previously serialized numeric values remain stable.
Interface template keys use the existing nominal module/namespace/name/arity identity and the
specialization cache retains one entry per closed interface.

### Reutilización del pipeline / Pipeline reuse

No se crea un segundo monomorfizador. La interfaz usa los mismos servicios que los otros tipos:

1. `cloneNode` sustituye `T` en contratos y bases;
2. `ProjectNameLookup` resuelve la declaración cerrada;
3. `ProjectInterfaceModel` construye herencia y tablas;
4. `ProjectGenericConstraintModel` valida constraints contra la interfaz cerrada;
5. `ProjectMethodBodyModel` demanda implementaciones concretas usadas por slots;
6. HIR/MIR/LLVM emiten el dispatch existente;
7. `LibraryMetadata` publica plantilla, especializaciones, tipos y contratos de dispatch.

No parallel monomorphizer is introduced. Closed interfaces flow through the existing name,
inheritance, constraint, dispatch, HIR/MIR/LLVM, and metadata services.

## Contratos agregados / Added contracts

### `Stage44GenericsMonomorphizationTests.cpp`

El caso local exige que `IValue<int>` e `ITaggedValue<int>` produzcan exactamente dos
especializaciones de clase `Interface` y que la combinación de herencia, constraint, miembro
estático y dispatch alcance OOP MIR.

The local case requires exactly two interface specializations and requires interface inheritance,
constraints, a closed static member, and dispatch to reach OOP MIR.

### `Stage46NativeLibrariesTests.cpp`

Un productor O0 publica la jerarquía como biblioteca estática. Se verifican `.lib + .nsmeta`; un
consumidor O0 contiene sólo `LibraryReference`, crea las especializaciones cerradas, llama la API y
ejecuta con código 42. Esto cubre tanto especializaciones ya exigidas por el productor como la
primera especialización solicitada por otro módulo.

An O0 static producer publishes the hierarchy. Its archive and NSMETA are jointly verified; an O0
source-free consumer closes the types, invokes the API, and exits with 42. This covers producer-
demanded and consumer-first specializations.

## Archivos / Files

- `include/nativesharp/Frontend/ProjectGenericModel.h`: nueva clase de plantilla, anexada sin
  renumerar las existentes.
- `src/Frontend/ProjectGenericModel.cpp`: reconocimiento, clasificación y nombre de interfaces
  genéricas.
- `tests/unit/Stage44GenericsMonomorphizationTests.cpp`: contrato local acumulativo.
- `tests/unit/Stage46NativeLibrariesTests.cpp`: contrato precompilado source-free O0.

## Decisiones / Decisions

1. **Monomorfización nominal.** `IValue<int>` es un tipo cerrado real, no una interfaz abierta con
   sustitución tardía en LLVM.
2. **ABI conservada.** El nuevo discriminante se agrega al final; no cambia el significado de
   metadata existente.
3. **Constraints antes del cuerpo.** Un argumento inválido sigue fallando en el sitio de consumo.
4. **Dispatch autoritativo.** Las tablas de interfaz continúan siendo la autoridad de qué método
   concreto ocupa cada slot.
5. **Sin baseline intermedio.** Los contratos se reservan para el baseline final; tras el commit
   sólo se reconstruye `nsc` desde cero.

## Commit de referencia / Reference commit

**Título ES:** Completa miembros e interfaces genéricos

**Title EN:** Complete generic members and interfaces

**Descripción ES:** Incorpora interfaces al modelo de plantillas sin renumerar NSMETA y certifica
herencia, constraints, miembros estáticos cerrados y dispatch genérico local y precompilado.

**Description EN:** Add interfaces to the template model without renumbering NSMETA and certify
inheritance, constraints, closed static members, and local and precompiled generic dispatch.
