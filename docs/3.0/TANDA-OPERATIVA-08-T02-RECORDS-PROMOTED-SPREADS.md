# Tanda operativa 08 — T02-F04/F05/F06: records, promoted fields y spreads

## Objetivo

Convertir `record struct`, los parámetros primarios promovidos y los spreads de agregados de una
marca/helper parcial a una vertical ejecutable del compilador. El corte debe reutilizar el ABI de
`struct`, generar miembros sin identidad runtime, conservar orden de evaluación y fallar de forma
explícita donde todavía se necesitan moves parciales y drop flags.

## Hallazgo inicial

- `record struct` llegaba al AST únicamente como `aux="record"`; ninguna fase semántica le daba
  comportamiento distinto.
- `RecordDeriveSet` y `PromotedField` existían sólo como helpers de una prueba C++; no producían
  campos, constructores ni operaciones invocables.
- Todos los structs 2.x recibían implícitamente `Equals`, `GetHashCode` y `ToString`. Aplicar esa
  regla a records habría violado el requisito 3.0 de comportamiento generado sólo por opt-in.
- El spread generalizado sí tenía un vertical productivo para agregados de lifetime trivial:
  binding, HIR, MIR y LLVM evaluaban una fuente same-type una vez y copiaban sólo campos no
  reemplazados. Faltaba integrarlo y probarlo con records.
- La promoción de miembros de agregados C anónimos también era productiva, pero es un feature FFI
  diferente; no implementaba constructores primarios de structs/records.

## Sintaxis y desazucarado adoptados

```nativesharp
record struct Pair(int X, int Y = 2) derive(Equality, Hash, Print, Clone)
{
}

readonly record struct Id(int Value) derive(Clone)
{
}

struct Range(public int Start, private int End)
{
}
```

- La lista primaria requiere edición 3.
- En un `record struct`, un parámetro sin accesibilidad genera un campo `public`; en un `struct`
  ordinario genera un campo `private`. La accesibilidad escrita gana.
- `readonly` puede aplicarse al parámetro promovido; `readonly record struct` vuelve readonly todos
  sus campos primarios.
- Los defaults permanecen en el parámetro del constructor y no se convierten en inicializadores de
  campo.
- El parser desazucara la lista a campos físicos ordinarios y un constructor `public` ordinario.
  Su cuerpo ejecuta `this.Campo = parametro` en orden textual.
- Los nombres son los nombres físicos y depurables del layout; no existen capturas ni storage
  oculto.
- `derive(...)` acepta exactamente `Equality`, `Hash`, `Print` y `Clone`, sin duplicados.

## Cambios funcionales

### Records opt-in

- Los structs 2.x conservan su comportamiento anterior para compatibilidad.
- Un record sin derive no recibe operaciones implícitas.
- `Equality` habilita `Equals`, `==` y `!=` sobre el mismo tipo.
- `Hash` habilita `GetHashCode` sobre las mismas hojas escalares que usa igualdad.
- `Print` habilita `ToString` estático por nombre de tipo.
- `Clone` devuelve una copia por valor del receiver, sin heap, wrapper ni identidad runtime.
- Los derives parciales son válidos: `derive(Clone)` no habilita igualdad, hash ni print.
- Un método declarado por el usuario sigue ganando porque la fachada sintetizada sólo se consulta
  después de que falla el lookup ordinario.

### Promoted fields y constructor primario

- Campos y constructor entran al member model como declaraciones normales; layout, overloads,
  defaults, genéricos, HIR, MIR y LLVM no necesitan una ruta ABI paralela.
- Se validan nombres primarios duplicados y conflictos con miembros escritos en el cuerpo.
- Accesibilidad pública/privada se conserva en el símbolo de campo.
- `record struct Box<T>(T Value) derive(Clone)` conserva campos, constructor y derive después de
  la especialización Stage 44.
- Records anidados no genéricos usan el mismo desazucarado; la limitación histórica de tipos
  anidados genéricos se conserva y se diagnostica por separado.
- La metadata de agregado del template ahora sobrevive en el clon cerrado después del delimitador
  reservado `;nsc-meta:` que sigue al prefijo/clave `stage44-specialization`; los lectores de la
  clave canónica recortan esa metadata adicional sin confundir comas entre argumentos genéricos.
  Esto también evita perder futuras marcas de layout/union/view de structs genéricos.

### Spreads

- La forma `{ X = replacement, ..source }` funciona sobre records de lifetime trivial.
- `source` se evalúa exactamente una vez.
- Los campos explícitos reemplazan los campos correspondientes del source sin duplicar lecturas.
- Se conserva el orden observable actual: los elementos se evalúan en orden textual y cada uno una
  vez; la fixture registra `replacement` y después `source` como `23`.
- El record usa el mismo `AggregateInitializer`/`AggregateSpread` HIR y el mismo layout de struct;
  no hay copia de padding como semántica de igualdad/hash.

## Defectos productivos corregidos durante la validación

### Ancho de hojas C8

El HIR describía correctamente las hojas escalares de igualdad/hash, pero el MIR guardaba el ancho
de una hoja sin hijo únicamente en `bitStorageSize`; LLVM leía `secondaryImmediate`. El resultado
era un ancho cero y el error `struct leaf 'builtin:int' has no scalar width`. El MIR ahora publica
el ancho en ambos campos requeridos, manteniendo la metadata de bit storage y alimentando al
lowerer correcto.

### Derives en records genéricos

Stage 44 sustituía todo `auxiliaryText` del clon con la clave de especialización, borrando la marca
`record,derive=...`. La especialización conserva ahora su prefijo canónico y añade después la
metadata del template con un delimitador reservado. MIR, HIR y CodeView extraen sólo la clave
anterior al delimitador. Además, el
modelo de cuerpos recibe explícitamente el catálogo genérico y puede consultar el template origen.

## Límites deliberados y dependencias

- Parámetros primarios `owned`/`borrowed`, spreads move-only, partial moves, lectura posterior de
  campos movidos y drop flags se rechazan o permanecen fuera del corte. Sus dueños son las tandas
  operativas 13–14; aceptarlos ahora produciría doble drop o ownership invisible.
- Las tuplas move-only y los partial drops siguen abiertos en T06.
- Los derives públicos ya se conservan en NSMETA 1.26 y el consumidor precompiled reconstruye la
  misma política del productor.
- Debugger y LSP pertenecen a la tanda 29.
- La promoción C de campos anónimos preexistente se conserva; no se mezcla con parámetros
  primarios.
- Por estas dependencias el gate T02 completo sigue `IMPLEMENTING`, aunque la tanda operativa 08
  queda terminada.

## Archivos nuevos

- `tests/cc1/c58_ns3_records/c58_ns3_records.nsproj`: proyecto permanente Language 3.
- `tests/cc1/c58_ns3_records/Main.ns`: fixture O0 end-to-end para records, derives, defaults,
  readonly, genéricos, visibilidad, constructor primario y spread.
- Este documento: evidencia, decisiones, archivos, mejoras, límites y validación de la tanda.

## Archivos modificados

- `include/nativesharp/Parser/Parser.h`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Parser/Parser.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Frontend/ProjectGenericModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `tests/unit/NativeSharp3T02AggregateTests.cpp`
- `tests/unit/Cc1CorrectnessTests.cpp`
- `docs/3.0/TANDA-02-TUPLAS-DECONSTRUCCION-Y-REGISTROS.md`

## Mejora obtenida

NativeSharp 3.0 ya puede expresar records ligeros con costo visible: sus campos son el layout, su
constructor es un constructor real, sus derives son opt-in y su clone es una copia de valor. No se
introduce heap, vtable, boxing, reflection ni runtime administrado. El mismo programa funciona para
records cerrados y genéricos, y el spread reutiliza el camino productivo de agregados sin repetir la
fuente. Los casos de ownership aún inseguros fallan en frontend con una dependencia concreta.

## Validación dirigida

- Build exclusivo `nsc`, configuración Debug/O0, 12 jobs: correcto.
- Compilador ejecutado: `A:\nsc-devel\build-clang\bin\nsc.exe`.
- `c58_ns3_records`, O0: compila/enlaza y termina con código 42.
- La fixture cubre constructor y efectos izquierda→derecha, campos públicos/privados, default,
  readonly, record anidado, records genéricos de uno/dos argumentos, derives completos/parciales,
  clone, equality/hash/print y spread con fuente única.
- Dump de layouts: `Pair` 8/4, `Promoted` 8/4, `CloneOnly` y `ReadonlyValue` 4/4; nombres y offsets
  físicos coinciden con los campos promovidos.
- Dump HIR: constructores ordinarios, `aggregate-equals`, `aggregate-hash` y
  `aggregate-spread ... source-evaluations=1`.
- Probes negativos dirigidos:
  - record sin derive: `Clone` desconocido y `==` exige `derive(Equality)`;
  - primary/derive inválido: campo duplicado, derive desconocido y derive duplicado;
  - ownership primario: rechazo causal hasta T13/T14;
  - shadowing: miembro escrito colisiona con el campo promovido y señala ambas declaraciones.
- El target unitario T02 no estaba configurado en `build-clang`; no se reconfiguró el árbol ni se
  ejecutó CTest. La prueba fuente quedó actualizada y la fixture queda registrada para el baseline
  final de Alberto.
- No se ejecutó baseline, O1/O2/O3, instalación, deploy, remoto ni nada en `C:\native-sharp`.

## Cierre itemizado F04 — 2026-10-03

La implementación histórica quedó endurecida como `NS3-T02-F04`: validación recursiva de records
anidados, rechazo de Clone sobre ownership, semántica definida para records vacíos y publicación
de record/derives en NSMETA 1.26. La batería exclusiva `nsc_ns3_t02_native_record_tests` pasa
12/12 con O0 y NSLink local; el consumidor precompilado ejecuta con exit 42. F05 y F06 conservan
su tracking independiente y no se dan por terminados mediante este cierre.

## Cierre itemizado F05 — 2026-10-03

La sintaxis de parámetros primarios quedó cerrada como contrato independiente: el desazucarado
produce campos y constructor ordinarios, conserva orden, defaults, `readonly`, accesibilidad,
genéricos, layout y nombres CodeView, y rechaza colisiones u ownership invisible. La batería
exclusiva `nsc_ns3_t02_promoted_field_tests` pasa 11/11 con O0 y NSLink local. F06 conserva su
tracking independiente y no se da por terminado mediante este cierre.
