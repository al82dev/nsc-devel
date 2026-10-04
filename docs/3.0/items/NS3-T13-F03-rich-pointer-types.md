# NS3-T13-F03 — Tipos de puntero enriquecidos / Rich pointer types

## Español

NativeSharp 3.0 incorpora contratos semánticos por cada nivel exterior de puntero sin cambiar
su representación física de Windows x64 ni el uso de punteros opacos de LLVM:

- `T*?`: puntero explícitamente anulable; el `T*` histórico conserva esta semántica.
- `T*!`: puntero no nulo.
- `T const*`: destino apuntado de sólo lectura.
- `T* align(N)`: alineación conocida, con `N` potencia de dos entre 1 y 4096.
- `T* provenance(tracked|none)`: procedencia conservada o explícitamente borrada.
- `T* addrspace(N)`: espacio de direcciones reservado entre 0 y 255.

Los contratos se preservan en parámetros, retornos, campos, locales, resultados de llamadas,
condicionales y aritmética. Una conversión implícita sólo puede debilitar garantías: no puede
inventar no-nulidad, mutabilidad, procedencia, alineación ni cambiar el espacio de direcciones.
Un cast que fortalece o altera esas garantías exige un bloque `unsafe` auditable. La aritmética
conserva procedencia y espacio de direcciones, pero reduce la alineación conocida a uno cuando
el desplazamiento no se demuestra múltiplo de la alineación.

Los dumps de miembros y cuerpos publican los cinco ejes del contrato para revisión de ABI y
herramientas. Estos calificadores no cambian el `typeKey`, el layout, el calling convention ni
la firma binaria; son promesas de seguridad y optimización verificadas por semántica.

Operaciones que siguen siendo indefinidas o inseguras incluyen desreferenciar `null`, usar una
dirección fuera del objeto o one-past, mentir sobre alineación/procedencia mediante un cast y
tratar memoria de otro espacio reservado como si perteneciera al espacio cero. `unsafe` hace
explícita la responsabilidad; no convierte una operación inválida en válida.

Validación dirigida: 10 comprobaciones cubren la retícula de garantías, ejecución O0 enlazada
con NSLink, rechazo de `null`, fortalecimiento de alineación, casts auditados, procedencia al
cruzar una llamada, espacios de direcciones, alineaciones inválidas y aritmética conservadora.

## English

NativeSharp 3.0 adds semantic contracts to each outer pointer level without changing its
Windows x64 physical representation or LLVM opaque-pointer lowering:

- `T*?`: explicitly nullable pointer; legacy `T*` keeps this behavior.
- `T*!`: non-null pointer.
- `T const*`: read-only pointee.
- `T* align(N)`: known alignment, where `N` is a power of two from 1 through 4096.
- `T* provenance(tracked|none)`: preserved or explicitly erased provenance.
- `T* addrspace(N)`: reserved address space from 0 through 255.

Contracts flow through parameters, returns, fields, locals, call results, conditionals, and
pointer arithmetic. An implicit conversion may only weaken guarantees: it cannot invent
non-nullness, mutability, provenance, or alignment, and it cannot change address spaces. A cast
that strengthens or changes those guarantees requires an auditable `unsafe` block. Arithmetic
preserves provenance and address space, while conservatively reducing known alignment to one
unless an offset multiple is proven.

Member and method-body dumps expose all five contract axes for ABI and tooling audits. The
qualifiers do not alter `typeKey`, layout, calling convention, or binary signatures; they are
semantic safety and optimization promises.

Undefined or unsafe behavior still includes dereferencing `null`, using an out-of-object or
one-past address, lying about alignment/provenance through a cast, and treating another reserved
address space as address space zero. `unsafe` makes responsibility explicit; it does not make an
invalid operation valid.

The directed test contains 10 checks for the guarantee lattice, O0 execution linked by NSLink,
null rejection, alignment strengthening, audited casts, call-boundary provenance, address
spaces, invalid alignment, and conservative pointer arithmetic.
