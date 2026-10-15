# NS304-T13-F07 — Provenance intermodular/DLL

- **Estado**: EN CURSO — núcleo IMPLEMENTADO (provenance en header + deallocate por allocator); pendientes: fallo de ctor y cross-module.
- **Rama**: development

## Hueco concreto (demostrado)

`owned A a = new (alloc) A(42);` → al droppear `a`, `alloc.Deallocate` no se llamaba (`Deallocs==0`).
**Cerrado**: ahora `Deallocs==1` y `Drops==1`; el objeto conserva el allocator en su header.

## Implementado (commits)

- `74ce52ef` FASE 1: contrato ABI (header prependido: descriptor@0, context@8, thunk@16; payload@24).
- `dced1bfb` FASE 2: `ClassObjectLayout` + offsets + tamaño (+16B) con autoridad única; herencia/campos OK.
- `7726a11e` FASE 3a: provenance default (null + free thunk) en construcción; thunk `void(void*,void*)` internal.
- `8c8f54c1` FASE 3b+4: provenance custom (context + thunk por tipo de allocator) + deallocación unificada (campo 7 + thunk).
- `5c4a11bb` test focal TL-LANG-492 (drop llama `Deallocate` del mismo allocator; herencia + no-doble-free).

## Verificado O0

- Custom allocator: `Deallocs==1` por drop; mismo allocator (context en header). PASS.
- Default `new T()`: comportamiento previo (destructor + free). PASS.
- Herencia (`B : A`): destructor base + derivado, `Deallocs` correcto. PASS.
- No-doble-free: drop una vez. PASS.

## Pendiente

1. **Fallo de constructor** (focal #6): si el ctor lanza tras `Allocate`, la reserva debe recuperarse con la
   MISMA provenance. Hoy `Deallocs` no se incrementa en ese camino (fuga del bloque del allocator). Requiere
   una región de cleanup sobre el storage enganchada al unwind del ctor.
2. **Escape/retorno** (focal #3): el retorno de un placement-constructed object está bloqueado por un error
   de surface previo ("owned return requires an ownership-transfer expression"), no por la provenance.
3. **Cross-module** (FASE 5): thunk ABI estable ya definido; falta la prueba productor/consumidor.
4. **Vida del allocator** (FASE 6): regla contractual documentada (contexto crudo; entidad estable en pruebas).

## ABI de provenance (fijo)

`void deallocate_thunk(void* context, void* ptr)`; default `context=null, thunk=free`; custom
`((T*)context)->Deallocate((byte*)ptr)` generado por tipo de allocator (internal linkage).
