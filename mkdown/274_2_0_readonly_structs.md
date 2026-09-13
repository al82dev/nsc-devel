# 274 — Structs readonly / Readonly structs

## Resumen / Summary

NativeSharp incorpora `readonly struct` como propiedad semántica verificable del tipo. Todos los
campos de instancia deben ser readonly, el constructor declarante puede inicializarlos y cualquier
escritura posterior permanece prohibida. La propiedad se conserva al publicar y consumir NSMETA.

NativeSharp adds `readonly struct` as a verifiable semantic property of the type. Every instance
field must be readonly, the declaring constructor may initialize it, and later writes remain
forbidden. The property survives NSMETA publication and consumption.

## Archivos y funciones / Files and functions

- `src/Parser/Parser.cpp` — reconoce el modificador en structs globales y anidados, lo conserva en
  el AST y rechaza clases, interfaces, enums, delegates y unions.
- `src/Frontend/ProjectMemberModel.cpp` — exige `readonly` en cada campo de instancia del tipo;
  deja fuera de esa invariante el almacenamiento estático.
- `src/Interop/LibraryMetadata.cpp` — publica la propiedad en el bit 6 de las banderas de tipo.
- `src/Driver/Driver.cpp` — reconstruye `readonly struct` al materializar la API pública de NSMETA.
- `tests/unit/ReadonlyStructTests.cpp` — verifica construcción, campos mutables, mutación posterior
  y formas de tipo inválidas.
- `tests/unit/ClassMemberSyntaxTests.cpp` — actualiza el contrato de tipos anidados.
- `CMakeLists.txt` — registra la suite dirigida.
- `docs/1.0.1/` — actualiza especificación, referencia, estado, changelog, mapas y roadmap.

## Implementación y decisiones / Implementation and decisions

La inmutabilidad se guarda en la declaración nominal, no se infiere observando accidentalmente sus
campos. A la vez, cada campo de instancia debe repetir `readonly`: esto permite que las reglas ya
existentes de asignación identifiquen la ubicación exacta que sólo puede escribirse durante el
constructor del tipo declarante. No se sintetizan setters ni se transforma almacenamiento.

Immutability is stored on the nominal declaration rather than inferred accidentally from its
fields. Each instance field must also repeat `readonly`, allowing the existing assignment rules to
identify the exact storage location writable only during the declaring type's constructor. No
setters are synthesized and storage is not transformed.

Los campos estáticos se excluyen porque no integran el valor copiado. Un readonly struct conserva
exactamente el mismo tamaño, alineación, offsets y convención de llamada que el struct equivalente
sin el modificador. Las reglas de receptor readonly se reservan para el item 275: este commit no
introduce copias defensivas ni pretende cerrar mutaciones transitivas mediante llamadas.

Static fields are excluded because they are not part of the copied value. A readonly struct keeps
the exact size, alignment, offsets, and calling convention of the equivalent unmodified struct.
Readonly receiver rules remain item 275: this commit introduces no defensive copies and does not
claim to close transitive mutation through calls.

La bandera de tipo viaja en NSMETA, aunque no cambia el contrato físico. Es necesaria para que el
consumidor no acepte escrituras que el productor prohibía. La reconstrucción pública vuelve a
emitir el modificador antes de `struct`, y la validación del consumidor comprueba de nuevo todos los
campos publicados.

The type flag travels through NSMETA even though it changes no physical contract. A consumer must
not accept writes forbidden by the producer. Public-source reconstruction emits the modifier before
`struct`, and consumer validation checks all published fields again.

## Evidencia dirigida / Focused evidence

- Un constructor inicializa dos campos readonly mediante nombre directo y `this`.
- Un campo estático mutable es legal porque no pertenece al valor.
- Un campo de instancia no readonly invalida el tipo.
- Un método ordinario que reasigna un campo readonly recibe `InvalidAssignmentTarget`.
- `readonly class` y `readonly union` se rechazan.
- Un struct anidado conserva el modificador en el AST.
- Las dos suites dirigidas agregan y pasan 8 comprobaciones específicas.
- Una biblioteca estática mínima O0 publica el struct en NSMETA, supera
  `--verify-library-artifacts` y permite que un consumidor externo lea sus campos.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade structs readonly / Add readonly structs`

**Descripción / Description**

`Hace readonly una propiedad verificable del struct, exige campos de instancia readonly, conserva la inicialización en constructores y publica la inmutabilidad en NSMETA. / Makes readonly a verifiable struct property, requires readonly instance fields, preserves constructor initialization, and publishes immutability through NSMETA.`
