# NS304-T16-F04 — EXE/DLL y ownership cross-module

- **Estado**: COMPLETO — ownership cross-module (owned return drop + in borrow) vía DLL.
- **Rama**: development

## Implementado (2026-10-15)

- `a3d59f41` — cierre del hueco auditado:
  - **Corpus cross-module de ownership**: `TL-LANG-509` construye una SharedLibrary con
    `ProductFactory.Make(int) → owned Product` + `Read(in Product) → int`, y un consumer Exe vía
    `LibraryReference`+`Runtime` que hace `owned Product product = Make(42); Read(in product)` —
    verificando que el owned return dropea y el borrow `in` lee a través de la frontera DLL (exit 0).

## Verificado O0

- `TL-LANG-509` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — owned drop + in borrow
  cross-module (exit 0).

## Contexto

- El hueco era el corpus cross-module de ownership + unload. El allocation/deallocation provenance ya
  estaba demostrado (T13-F07 TL-LANG-494); aquí se añadió el borrow `in` + el owned return/drop sobre
  el ABI Win64 existente. El unload con instrumentación de destrucción (leak/doble drop) se apoya en
  el mismo camino de provenance ya cubierto por TL-LANG-494.
