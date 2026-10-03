# Tanda operativa 12 — T05 traits y despacho explícito

> **Actualización 2026-10-03:** este documento conserva la evidencia histórica de la primera
> vertical. `NS3-T05-F01` cerró después métodos default, requisitos `static`, conflictos y el
> marcador/orden de slots en NSMETA 1.28. Las frases posteriores que los describen como deuda se
> leen como estado de aquella entrega, no como estado actual. Véanse
> [TANDA-05-TRAITS-Y-DESPACHO.md](TANDA-05-TRAITS-Y-DESPACHO.md) y
> [NS3-T05-F01-traits.md](items/NS3-T05-F01-traits.md). `NS3-T05-F02` cerró después bloques
> `impl`, coherencia sellada, impl condicional y transporte NSMETA; véase
> [NS3-T05-F02-impl-coherence.md](items/NS3-T05-F02-impl-coherence.md).
> `NS3-T05-F06` cerró posteriormente extensiones estáticas, lookup explícito y NSMETA 1.32; véase
> [NS3-T05-F06-extension-methods.md](items/NS3-T05-F06-extension-methods.md). Las frases históricas
> que dejan extension methods abiertas describen solamente el alcance de esta primera vertical.
> `NS3-T05-F07` cerró después la tabla lang-item, witnesses estáticos, referencias implícitas y
> codegen inline de operadores; véase
> [NS3-T05-F07-operator-traits.md](items/NS3-T05-F07-operator-traits.md). Las frases históricas que
> dejan operadores por traits abiertos describen también solamente el alcance inicial.

## Objetivo

Conectar una primera vertical segura de T05 al compilador productivo Windows x64: declaraciones
`trait`, constraints genéricos con despacho estático monomorfizado y tipos `dyn Trait` con despacho
dinámico visible. La entrega debía reutilizar el modelo de interfaces, tablas y metadata ya probado,
no crear un segundo ABI paralelo, llegar a PE O0 y cruzar `.lib/.nsmeta`.

Esta tanda no publica `impl`, associated items, `some Trait`, extension methods ni operadores por
traits. Esas piezas requieren coherence/lookup/metadata nuevos y continúan abiertas; simularlas con
helpers habría producido dos sistemas semánticos incompatibles.

## Situación encontrada

- `TraitSemantics` describía coherence, object safety, proyecciones, `some`, `dyn`, extensiones y
  operadores, pero ningún elemento era consumido por parser, lookup, HIR, MIR o NSMETA.
- NativeSharp 2.x ya tenía interfaces nominales, herencia de interfaces, constraints sobre
  interfaces, especialización genérica, tablas de métodos y llamadas dinámicas hasta LLVM.
- El contrato histórico del helper proponía un fat pointer de 16 bytes, pero ese ABI nunca estuvo
  integrado. El producto real usa el ABI de referencia/interfaz ya establecido; cambiarlo en esta
  tanda habría roto metadata y bibliotecas sin un gate DLL completo.
- No había sintaxis que hiciera visible en fuente la diferencia de costo entre una llamada
  monomorfizada y una llamada por tabla de interfaz.

## Diseño implementado

### `trait` como contrato nominal storage-free

`trait` es contextual y exclusivo de edition 3. Se parsea por la ruta productiva de
`InterfaceDeclaration`; por ello herencia, accesibilidad, constraints, generic templates,
implementación por una clase, tablas, lowering y metadata usan una sola semántica.

El AST conserva `auxiliaryText=ns3-trait` en la declaración fuente. Un trait no puede declarar
campos porque el parser de interfaces sólo admite contratos de método/propiedad/indexer. Tampoco
acepta modificadores `abstract`, `sealed` o `static` en el tipo.

La vertical actual garantiza object safety rechazando métodos genéricos dentro de un trait y
recomienda mover el parámetro al trait (`trait IMap<T>`), forma que sí atraviesa Stage 44 y dispatch.
Default methods y `Self` por valor todavía no se publican.

### Despacho estático

`where T : Trait` reutiliza el solver de constraints T04. Cuando una llamada cierra T con un tipo
concreto, Stage 44 clona el cuerpo y MIR contiene `direct-instance-call` al miembro concreto. No hay
boxing, tabla ni diccionario genérico runtime.

### `dyn Trait` y despacho dinámico

`dyn` es un calificador contextual edition 3 conservado por `TypeSyntax`, aliases y clonación
genérica. No cambia la identidad canónica: `dyn IScore` y `IScore` comparten el ABI de interfaz, pero
el AST/dump conserva la escritura explícita y name lookup exige que el destino sea interfaz/trait.
`dyn int`, `dyn tuple` y `dyn Class` fallan con diagnóstico causal.

En MIR, una conversión concreta→trait aparece como `reference-interface`; las llamadas aparecen
como `interface-instance-call` y se publican las `interface-binding`/method tables. Así, el costo
dinámico es inspeccionable sin introducir una representación experimental.

### Frontera `.nsmeta`

Un productor `StaticLibrary` con trait y clase implementadora fue emitido como `.lib/.nsmeta`. La
API pública se rehidrata usando el contrato de interfaz compatible; un consumidor edition 3 declara
`dyn IScore`, construye la clase del productor, enlaza el archive y devuelve 42. La semántica y ABI
cruzan el módulo, aunque el marcador fuente `trait` todavía no tiene un campo propio en NSMETA.

## Cambios por archivo

### Modificados

- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`
  - reconocen `trait` contextual edition 3 y lo envían al parser productivo de interfaces;
  - reconocen `dyn` en el scanner/parser de tipos sin reservar el identificador en 2.x;
  - rechazan modificadores y métodos genéricos no object-safe en esta vertical.
- `include/nativesharp/AST/TypeSyntax.h`, `src/AST/TypeSyntax.cpp`
  - conservan el marcador source-level de despacho dinámico.
- `src/AST/AstDumper.cpp`
  - reproduce `dyn` en el dump de tipos.
- `src/Frontend/ProjectGenericModel.cpp`
  - preserva `dyn` al expandir aliases y clonar especializaciones.
- `src/Frontend/ProjectNameLookup.cpp`
  - valida que `dyn` sólo apunte a un tipo interface/trait resuelto.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra c62 para el baseline final que ejecutará Alberto.
- `docs/3.0/TANDA-05-TRAITS-Y-DESPACHO.md`
  - reemplaza el estado de helper aislado por la vertical productiva y sus límites reales.

### Nuevos

- `tests/cc1/c62_ns3_traits_dispatch/c62_ns3_traits_dispatch.nsproj`
  - fixture mínima edition 3.
- `tests/cc1/c62_ns3_traits_dispatch/Main.ns`
  - traits simples/heredados/genéricos, constraint estático y tres vistas `dyn`; devuelve 42.
- `docs/3.0/TANDA-OPERATIVA-12-T05-TRAITS-Y-DESPACHO.md`
  - handoff técnico, evidencia, archivos y deuda del commit.

No se creó código fuente fuera de `A:\nsc-devel`. Proyectos, ejecutables, bibliotecas, metadata y
logs temporales se eliminan antes del commit.

## Mejora concreta

- NativeSharp 3.0 ya expresa contratos storage-free como `trait` sin duplicar el ABI de interfaces.
- El autor distingue en fuente `T : Trait`/monomorfización de `dyn Trait`/tabla dinámica.
- Traits genéricos e inheritance usan las identidades y tablas deterministas existentes.
- Usos `dyn` inválidos fallan durante name lookup y no llegan al backend.
- La selección estática y la dinámica son visibles de manera diferente en MIR.
- La vertical funciona igual desde fuente y desde un productor precompilado.

## Validación dirigida ejecutada

1. Construcción únicamente de `nsc`, configuración Debug/O0, target `nsc`: correcta.
2. `c62_ns3_traits_dispatch` con `--emit-project-oop-exe -O0`: correcta; exit 42.
3. MIR dirigido:
   - una especialización `StaticScore<Number>`;
   - llamada interna `direct-instance-call` a `Number.Score`;
   - tres `interface-instance-call` para `IScore`, `IDoubled` e `IMapper<int>`;
   - tres `interface-binding` con tablas y slots deterministas.
4. Productor `.lib/.nsmeta` → consumidor separado con `dyn IScore`: emisión, enlace y ejecución
   correctos; exit 42.
5. Ocho negativos individuales con exit distinto de cero y causa esperada: trait edition 2, dyn
   edition 2, dyn class, dyn builtin, storage en trait, contrato faltante, static trait y método
   genérico no object-safe.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota.

## Límites y deuda explícita

- `trait` usa hoy el ABI productivo de interface reference; no se afirma el fat pointer de 16 bytes
  descrito por el helper histórico. Un ABI alternativo exige diseño/versionado y witness DLL.
- La implementación se declara en la lista base de la clase, por lo que no existe aún sintaxis
  `impl`; eso impide impls huérfanos por construcción, pero tampoco ofrece conditional/external impls.
- Default methods, associated types/constants, `Self`, `some Trait` y object-safety completa siguen
  abiertos.
- Extension methods y operadores por traits no están conectados a lookup/resolution.
- NSMETA conserva el contrato/ABI como interface, pero no todavía el origen léxico `trait` ni la
  explicación de selección que requerirán LSP/debugger.

La tanda operativa 12 queda cerrada con una vertical F01/F02/F05 productiva y conservadora; T05
maestro permanece `IMPLEMENTING` hasta cerrar F03/F04/F06/F07, coherence ampliada, ABI/tooling y el
gate DLL completo.
