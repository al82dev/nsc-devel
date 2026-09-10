# 1.0.1 — Genéricos entre DLLs / Generics across DLLs

## Resultado / Outcome

**ES.** DLL-P3 amplía el gate source-free de bibliotecas precompiladas con
overloads de método genérico, parámetros predeterminados y transferencia
`owned/move` sobre una clase importada. Los consumidores estático y DLL ejecutan
las especializaciones en O0/O2.

**EN.** DLL-P3 expands the source-free precompiled-library gate with generic
method overloads, default parameters, and `owned/move` transfer over an imported
class. Static and DLL consumers execute the specializations at O0/O2.

## Cobertura consolidada / Consolidated coverage

- template de tipo y método; especialización publicada y nueva;
- constraints de clase e interfaz;
- argumentos externos, nested y transitivos;
- implementaciones de interfaz demandadas únicamente por tablas;
- overloads, defaults y ownership en firmas genéricas;
- ambigüedad de módulos y rechazo de métodos privados.

- type and method templates; published and new specializations;
- class and interface constraints;
- external, nested, and transitive arguments;
- interface implementations demanded only by tables;
- overloads, defaults, and ownership in generic signatures;
- module ambiguity and private-method rejection.

## Archivos / Files

- `tests/unit/Stage46NativeLibrariesTests.cpp`: `Pick<T>` overloads,
  `AddDefault<T>` y `Transfer<T>` con ejecución en consumidores estático y DLL.

## Evidencia / Evidence

```text
nsc_stage46_native_libraries_tests, lld-link: 151/151 PASS
nsc_stage46_native_libraries_tests, NSLink:   151/151 PASS
optimization: O0 and O2
build root: A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
```

No se instaló producto ni se usó `A:\nsc-devel`.
No product was installed and `A:\nsc-devel` was not used.

## Commit / Commit

```text
Endurece genéricos entre DLLs NativeSharp / Harden generics across NativeSharp DLLs
```
