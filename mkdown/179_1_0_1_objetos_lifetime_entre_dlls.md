# 1.0.1 — Objetos y lifetime entre DLLs / Objects and lifetime across DLLs

## Resultado / Outcome

**ES.** DLL-P4 agrega una cadena mixta real: `LocalCounter`, compilada en el
consumidor, deriva de `CounterBase` precompilada e implementa `IScore` importada.
El gate construye el descriptor local sobre el accessor PE del productor y
ejecuta el override mediante receptor concreto, base e interfaz en O0/O2.

**EN.** DLL-P4 adds a real mixed chain: consumer-compiled `LocalCounter` derives
from precompiled `CounterBase` and implements imported `IScore`. The gate builds
the local descriptor on the producer's PE accessor and executes the override
through concrete, base, and interface receivers at O0/O2.

## Cobertura consolidada / Consolidated coverage

- construcción y base initializer entre módulos;
- descriptor accessor, vtable e interface table;
- destrucción exactamente una vez y ownership `owned/borrowed/move`;
- delegates estáticos, de instancia y closures;
- throw/catch/finally tipado a través de DLL;
- ejecución O0/O2 con lld-link y NSLink.

- cross-module construction and base initializer;
- descriptor accessor, vtable, and interface table;
- exactly-once destruction and `owned/borrowed/move` ownership;
- static, instance, and closure delegates;
- typed throw/catch/finally across a DLL;
- O0/O2 execution with lld-link and NSLink.

## Archivos / Files

- `tests/unit/Stage46NativeLibrariesTests.cpp`: clase local derivada, override y
  tres rutas de dispatch comprobadas en ejecución.

## Evidencia / Evidence

```text
nsc_stage46_native_libraries_tests: 151/151 PASS (lld-link)
nsc_stage46_native_libraries_tests: 151/151 PASS (NSLink)
build root: A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
```

No se instaló producto ni se usó `A:\nsc-devel`.
No product was installed and `A:\nsc-devel` was not used.

## Commit / Commit

```text
Amplía objetos y lifetime entre DLLs / Expand objects and lifetime across DLLs
```
