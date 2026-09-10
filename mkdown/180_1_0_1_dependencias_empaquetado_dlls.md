# 1.0.1 — Dependencias y empaquetado de DLLs / DLL dependencies and packaging

## Resultado / Outcome

**ES.** DLL-P5 formaliza el empaquetado local de runtimes. El compilador parte
exclusivamente de los `Runtime` declarados por `LibraryReference`, valida su
identidad física y sigue sólo nombres presentes en la tabla de imports PE que
existan junto a la DLL propietaria. No consulta `PATH` ni escanea directorios.

**EN.** DLL-P5 formalizes local runtime packaging. The compiler starts only from
`Runtime` paths declared by `LibraryReference`, validates their physical
identity, and follows only PE import-table names that exist beside the owning
DLL. It neither queries `PATH` nor scans directories.

## Cobertura / Coverage

- una DLL productora conserva una dependencia NativeSharp dinámica privada;
- el consumidor enlaza únicamente la DLL superior y no redeclara la transitiva;
- la clausura exacta copia ambas DLLs junto al ejecutable;
- lld-link construye y ejecuta el paquete desde una ruta Unicode fuera del
  checkout;
- NSLink construye y ejecuta la misma clausura desde una ruta externa ASCII,
  acorde con su interfaz de rutas actual;
- dos contratos que reclaman archivos distintos con el mismo basename fallan
  antes de enlazar o copiar;
- el empaquetado rechaza una dependencia que no sea PE32+ AMD64 DLL.

- a producer DLL retains a private dynamic NativeSharp dependency;
- the consumer links only the top-level DLL and does not redeclare the
  transitive dependency;
- the exact closure copies both DLLs beside the executable;
- lld-link builds and runs the package from an out-of-checkout Unicode path;
- NSLink builds and runs the same closure from an external ASCII path, matching
  its current path interface;
- two contracts claiming different files with the same basename fail before
  linking or copying;
- packaging rejects a dependency that is not a PE32+ AMD64 DLL.

## Evidencia / Evidence

```text
nsc_stage46_native_libraries_tests: 157/157 PASS (lld-link)
nsc_stage46_native_libraries_tests: 157/157 PASS (NSLink)
build root: A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink
```

No se instaló producto ni se usó `A:\nsc-devel`.
No product was installed and `A:\nsc-devel` was not used.

## Commit / Commit

```text
Formaliza dependencias y empaquetado de DLLs / Formalize DLL dependencies and packaging
```
