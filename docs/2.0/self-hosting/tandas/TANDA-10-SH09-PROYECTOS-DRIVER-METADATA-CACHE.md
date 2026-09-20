# NativeSharp 2.0 self-hosting — tanda 10: proyectos, driver, metadata y caché

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-113` a `NS2-SH-126` |
| Gate | `SH09` |
| Entrada | `ada216aca` |
| Commit | `selfhost(tanda-10): port projects driver metadata and cache` |
| Fecha | 2026-09-19 |

## 2. Resultado

`nsc-next` deja de ser sólo un compilador por archivo: carga y valida `.nsproj`, expone dump
canónico, acepta response files, selecciona O0–O3, resuelve la primera fuente del proyecto,
emite objeto, escribe/lee metadata compatible N-1/N y publica un manifiesto de toolchain con clave
de caché por contenido. El gate enlaza y ejecuta los cuatro objetos con `lld-link` y NSLink.

## 3. Diseño

```text
CLI / @response
       |
 ProjectLoader -- ProjectModel -- ProjectDumper
       |              |               |
       |          referencias      orden canónico
       v
 frontend + SSA -> LlvmBackend -> COFF -> lld-link / NSLink
       |
       +-> MetadataContract (NSMETA/1, NSMETA/2)
       +-> ProjectCache -> ToolchainManifest
```

El parser XML bootstrap es deliberadamente acotado: extrae propiedades y atributos normativos sin
incorporar un segundo motor XML. Copia todos los strings que sobreviven al texto fuente. Los paths
se resuelven respecto al directorio del proyecto; `**/*.ns` usa enumeración recursiva y elige en
orden ordinal. El dump vuelve a ordenar fuentes ordinalmente, independiente del orden Win32.

## 4. Tipos, métodos y funciones

| Elemento | Responsabilidad |
|---|---|
| `ProjectModel` | posee path, nombre, output, SDK, fuentes y referencias |
| `ProjectLoader.Tag` | extrae propiedad XML sin conservar vistas prestadas |
| `ProjectLoader.Attributes` | enumera `Compile` y `ProjectReference` |
| `ProjectLoader.FindFirstSource` | recorre directorios y selecciona `.ns` ordinal |
| `ProjectLoader.Load/Verify` | carga y valida forma/output/files |
| `ProjectLoader.FirstSourcePath` | resuelve include explícito o glob recursivo |
| `ProjectDumper.NextSource/Dump` | serialización `project schema=1` determinista |
| `MetadataContract.Write` | escribe NSMETA/2 con ABI y clase de salida |
| `MetadataContract.ReadCompatible` | acepta sólo NSMETA/1 o NSMETA/2 |
| `ProjectCache.Key` | combina dump, perfil y target mediante FNV-1a estable |
| `ToolchainManifest.Create` | target, runtime ABI, lenguaje, linker y cache key |
| `Program.RunResponseFile` | precedencia mínima de `@file`: acción + proyecto |
| `Program.ProjectCommand` | fachada dump/verify/metadata/manifest |
| `verify_project_driver.main` | valida matriz y hashes; reporte sólo en PASS |

## 5. Superficie CLI

```text
--dump-project <p.nsproj>
--verify-project <p.nsproj>
--emit-project-object <p.nsproj> -O0|-O1|-O2|-O3 -o <p.obj>
--emit-nsmeta <p.nsproj> <p.nsmeta>
--verify-nsmeta <p.nsmeta>
--emit-toolchain-manifest <p.nsproj> <manifest>
@response.rsp
```

## 6. Trazabilidad

| IDs | Evidencia |
|---|---|
| 113 | CLI, perfiles, `@response` y forma estricta |
| 114–115 | loader/validator, includes y orden ordinal |
| 116 | `ProjectReference` conservada; fixture App→Library |
| 117–118 | SDK, target, runtime ABI y lenguaje en contrato/manifiesto |
| 119 | writer v2 y reader N-1/N |
| 120 | output kinds Exe/Library/StaticLibrary/Facade validados |
| 121 | clave estable por dump/perfil/target |
| 122 | selección y dump deterministas; paralelismo todavía opcional |
| 123–124 | perfil, manifiesto y hashes de artefactos |
| 125 | ocho enlaces/ejecuciones por ambos linkers |
| 126 | gate corre junto a toda compatibilidad histórica de `nsc0` |

## 7. Archivos

- `selfhost/nsc/Projects/ProjectSystem.ns`: modelos, loader, dump, metadata, cache y manifest.
- `selfhost/nsc/Driver/Program.ns`: identidad SH09 y comandos.
- `selfhost/tests/projects/Sh09*`: aplicación y referencia reales.
- `selfhost/contracts/project-driver.contract.json`: matriz normativa.
- `selfhost/bootstrap/verify_project_driver.py`: verificador/reporte.
- `cmake/VerifyNativeSharpProjectDriver.cmake`: gate end-to-end.
- CMake, proyecto, READMEs, roadmap y pruebas de infraestructura: integración.

## 8. Gate específico

PASS aislado: cuatro `nsc-next`, cuatro dumps idénticos, cuatro objetos, cuatro `nsmeta`, cuatro
manifiestos, un response file equivalente, ocho enlaces y ocho ejecuciones con código 42. Los
artefactos se identifican por SHA-256. El gate no escribe en el árbol fuente.

## 9. Frontera honesta

El nuevo driver emite objeto de aplicación; la creación integral de LIB/DLL/facade y la compilación
transitiva completa todavía usa como oráculo la matriz histórica de `nsc0`. SH09 conserva esos
output kinds y referencias en el modelo, pero no declara falsamente que ya reimplementó todo el
packaging. Esa ampliación se prueba durante nsc1 y runtime en SH10–SH13.

## 10. Baseline

Corrida canónica `tanda-10-sh09-final-20260919`: `PASSED` en 774.58 s, instalada y
verificada exclusivamente en `A:\native-sharp`, con LLVM leído desde `C:\clang-llvm`.

| Matriz | Resultado |
|---|---:|
| CTest `lld-link` | 446/446 |
| CTest NSLink | 446/446 |
| fixtures por linker | 148/148 |
| triage por linker | 18/18 |
| gate SH09 | 36 comprobaciones, 4 perfiles, 2 linkers |

Identidad de la corrida:

- fuente S0: `f2b25cb4681b5403fa356007a8feb62db6f8203a646d4996978ef8aca77b68a7` (1418 archivos);
- `nsc0`: `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561`;
- NSLink: `acc4d5042cf9ad334346b957bcba32c452a3abb3cb2809ea0a5e3a8670b05c1a`;
- reporte JSON: `8a334b431e45d0bb8fda4e58f1ba00227a2a78405ac85706a29828469cd96620`;
- evidencia SH09 promovida: `278609f67c5520bf777f407d29f55037e2932563df7b388d916cad800de838df`.

La evidencia durable vive en `selfhost/evidence/gates/SH09/`; el reporte completo y sus logs
permanecen en `selfhost/evidence/runs/tanda-10-sh09-final-20260919/`.

## 11. Cierre

- [x] diseño, funciones, archivos y deuda documentados;
- [x] gate SH09 aislado verde;
- [x] no se escribió en `C:` ni se tocaron cambios ajenos;
- [x] baseline completo verde;
- [x] evidencia promovida;
- [x] commit exacto preparado: `selfhost(tanda-10): port projects driver metadata and cache`.
