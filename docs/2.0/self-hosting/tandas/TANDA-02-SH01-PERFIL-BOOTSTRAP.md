# NativeSharp 2.0 self-hosting — tanda 02: Bootstrap Profile 2.0

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-010` a `NS2-SH-019` |
| Gate | `SH01` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` (`6a9b94850`) |
| Commit de entrada | `69ee204fa` |
| Commit de cierre | este commit (`selfhost(tanda-02): freeze Bootstrap Profile 2.0`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-02-sh01-final2-20260919` |

## 2. Objetivo y alcance

### Objetivo

Congelar el subconjunto de lenguaje y biblioteca con que se escribirá `nsc-next`, convertirlo en un
contrato legible por herramientas y demostrar que el filtro acepta código representativo compilable
por `nsc0` y rechaza construcciones fuera de perfil antes del bootstrap.

### Incluido

- inventario normativo de sintaxis, tipos, genéricos, ownership, EH, punteros y stdlib;
- contrato JSON v1 y schema;
- verificador determinista previo al bootstrap;
- fixtures positiva y negativas con diagnósticos estables;
- pruebas unitarias y una prueba CTest que compila/ejecuta la fixture positiva con `nsc0`;
- Gate SH01 y baseline acumulativo SH00+SH01.

### No incluido

- portar módulos del compilador a NativeSharp;
- crear bindings LLVM o Win32;
- implementar un parser alterno en Python;
- paralelismo, optimización del compilador autoalojado o características 3.0.

## 3. Diseño

### Arquitectura

`BootstrapProfile-2.0.md` explica la intención humana; `bootstrap-profile.json` es la fuente
ejecutable. `verify_profile.py` valida su versión, tokeniza `.ns`, inspecciona `.nsproj` y produce
violaciones ordenadas. CTest ejecuta pruebas del contrato y después filtra, compila y ejecuta la
fixture positiva mediante el `nsc0` construido por la misma corrida.

Flujo: contrato → validación cerrada → colección ordenada → tokenizer sin comentarios/literales →
reglas de lenguaje/imports → referencias XML → reporte → sólo si pasa, compilación por `nsc0`.

### Contratos e invariantes

- contratos desconocidos fallan cerrados;
- códigos de salida: 0 conforme, 2 infracción, 3 entrada/contrato inválido;
- códigos `NSBPxxxx`, rutas, línea, columna y orden forman parte de la salida observable;
- ninguna escritura ocurre salvo que el llamador solicite `--report`;
- la fixture positiva debe devolver 42;
- el árbol `selfhost/nsc` se verifica en cada baseline aun antes de contener fuentes.

### Decisiones y alternativas

| Decisión | Alternativas consideradas | Razón |
|---|---|---|
| Filtro léxico pequeño + `nsc0` | segundo parser, regex directo | evita doble gramática; comentarios/literales no causan falsos positivos |
| JSON versionado | sólo Markdown | permite CI, diagnósticos y evolución fail-closed |
| Allowlist de proyectos stdlib | acceso irrestricto a `lib/` | hace observable una dependencia nueva |
| Single-thread inicial | autorizar Threading | evita que scheduling cambie IDs o salidas antes de SH13 |
| LLVM C permitido, intrinsics/asm prohibidos | backend C++ oculto | mantiene una frontera externa auditable sin circularidad |

## 4. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad | Razón |
|---|---|---|---|
| `CMakeLists.txt` | modificar | registrar dos CTests SH01 | el perfil forma parte del baseline normal |
| `nscbuilder.py` | modificar | emitir `report.json` v1 además de metadata/report Markdown | cerrar deuda observada de SH00 sin romper consumidores |
| `cmake/VerifyNativeSharpBootstrapProfile.cmake` | crear | filtrar, compilar y ejecutar fixture positiva | demuestra uso real de `nsc0` y retorno 42 |
| `docs/2.0/SELF_HOSTING_ROADMAP.md` | modificar | enlazar contrato, expediente y gate | mantener una fuente de plan vigente |
| `docs/2.0/self-hosting/BootstrapProfile-2.0.md` | crear | contrato humano completo | congelar decisiones e inventario |
| `docs/2.0/self-hosting/README.md` | modificar | descubrir el contrato | navegación documental |
| este expediente | crear | diseño, implementación y evidencia | metodología por tanda |
| `selfhost/bootstrap/bootstrap-profile.json` | crear | contrato ejecutable v1 | verificación automática |
| `selfhost/bootstrap/schemas/bootstrap-profile.schema.json` | crear | forma cerrada del JSON | evolución fail-closed |
| `selfhost/bootstrap/verify_profile.py` | crear | filtro previo al bootstrap | impedir fuera de perfil antes de compilar |
| `selfhost/bootstrap/README.md` | modificar | documentar comando y exits | operación reproducible |
| `selfhost/nsc/README.md` | modificar | hacer obligatorio SH01 para fuentes futuras | proteger el port |
| `selfhost/tests/README.md` | modificar | explicar corpus SH01 | separar positivos y negativos |
| `selfhost/tests/profile/positive/compiler_skeleton/*` | crear | proyecto representativo compilable | cobertura positiva por `nsc0` |
| `selfhost/tests/profile/negative/*` | crear | ocho rechazos y manifiesto | cobertura negativa por código estable |
| `tests/selfhost/test_bootstrap_profile.py` | crear | 10 pruebas del contrato/verificador | regresión rápida y portable |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | probar proyección al reporte público v1 | impedir otra corrida sin JSON normativo |
| `selfhost/evidence/gates/SH01/*` | crear | reporte y gate versionados | evidencia estable acumulativa |
| `MANIFEST_SHA256.txt` | modificar | hashes de fuentes cubiertas | integridad del repositorio |

## 5. Tipos y datos

| Tipo | Clase | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|---|
| `Token` | dataclass Python inmutable | lexema y posición física | línea/columna 1-based | valor del verificador |
| `Violation` | dataclass Python inmutable | diagnóstico `NSBP` estable | ordenable por ruta/posición/código | valor serializado al reporte |
| `ProfileError` | excepción Python | contrato o entrada inválida | produce exit 3 | capturada en CLI |
| profile JSON | contrato v1 | allowlists y reglas | schema/id/version exactos | archivo inmutable del commit |
| report JSON | contrato v1 | resultado consumible por CI | status deriva de violations | generado sólo bajo ruta solicitada |

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas y precondiciones | Salida y postcondiciones | Errores/diagnósticos | Efectos, ownership y concurrencia |
|---|---|---|---|---|---|
| `load_profile(path)` | leer y validar contrato v1 | JSON UTF-8 existente | dict validado | `ProfileError` si schema, versión o reglas no sirven | sólo lectura, thread-safe |
| `tokenize(text, source_name)` | producir tokens sin comentarios/literales | texto UTF-8 decodificado | posiciones deterministas | `ProfileError` en literal/comentario abierto | memoria local |
| `scan_source(path, profile, display_path)` | aplicar reglas `.ns` | perfil validado | lista ordenada de `Violation` | `NSBP1xxx/2xxx` | sólo lectura |
| `scan_project(path, profile, display_path)` | validar dependencias XML | `.nsproj` bien formado | violaciones de referencia | `NSBP3001/3002` o `ProfileError` | sólo lectura |
| `collect_inputs(roots)` | enumerar entradas | roots existentes | rutas absolutas únicas y ordenadas | `ProfileError` si falta un root | enumeración sin escritura |
| `verify(profile_path, roots)` | ejecutar verificación completa | contrato y roots válidos | reporte v1 | propaga `ProfileError` | sólo lectura |
| `write_report(path, report)` | persistir evidencia solicitada | destino administrado por llamador | JSON UTF-8 estable | error IO de Python | crea padres/destino |
| `main(argv)` | CLI y códigos de salida | argumentos válidos | 0, 2 o 3 | texto humano estable | single-thread |
| `structured_run_report(metadata)` | proyectar estado interno al reporte v1 | metadata final de una corrida | JSON con schema, inputs, etapas, casos y problemas | valores ausentes reciben defaults explícitos | función pura, sin IO |

## 7. Superficie observable

- CLI: `--profile`, `--source-root` repetible, `--report`, `--quiet`;
- exit 0/2/3 con significado documentado;
- contrato `nativesharp-bootstrap-profile` v1;
- reporte `nativesharp-bootstrap-profile-report` v1;
- diagnósticos `NSBP1099`, `NSBP2xxx` y `NSBP3xxx`;
- CTest: `nsc.selfhost.bootstrap-profile-contract` y
  `nsc.selfhost.bootstrap-profile-positive`.

## 8. Implementación

El expediente se creó antes del cierre funcional. La implementación mantiene separado el filtro de
perfil de la compilación real: primero se demuestra pertenencia al subconjunto y luego `nsc0`
resuelve gramática, tipos, ownership y codegen. La primera corrida completa descubrió que el runner
conservaba datos estructurados sólo como `metadata.json`; se añadió el `report.json` público v1 sin
retirar ese archivo compatible, y por ello se repite íntegramente el baseline.

## 9. Pruebas

| Prueba | Tipo | Propósito | Resultado | Evidencia |
|---|---|---|---|---|
| contrato/schema/versión | negativa | rechazar versión futura | PASS | `bootstrap-profile-contract` |
| fixture `compiler_skeleton` | positiva | clases, struct readonly, interface, enum, array, foreach, ownership y stdlib | PASS, retorno 42 | `bootstrap-profile-positive` |
| ocho fixtures fuera de perfil | negativa | assembly, threading, vectorcall, stackalloc, event, comptime, layout y union | PASS | unit tests |
| comentarios/literales | regresión | evitar falsos positivos | PASS | unit tests |
| referencias de proyecto | negativa | cerrar stdlib/dependencias externas | PASS | unit tests |
| orden/repetición | determinismo | diagnósticos idénticos | PASS | unit tests |

## 10. Comparación diferencial

La fixture positiva se compila con `nsc0`; el perfil no tiene todavía una generación NativeSharp
contra la cual comparar. El resultado esperado es retorno 42. Las fixtures negativas prueban el
filtro previo, no errores del lenguaje general: varias son NativeSharp 2.0 válidas pero quedan
deliberadamente fuera del bootstrap.

## 11. Baseline de cierre

| Campo | Valor |
|---|---|
| Comando | `python -B nscbuilder.py --baseline --config selfhost/bootstrap/bootstrap.config.example.json --run-id tanda-02-sh01-final2-20260919 --jobs 16` |
| Identificador | `tanda-02-sh01-final2-20260919` |
| Revisión probada | árbol de cierre de esta tanda sobre `69ee204fa` |
| Fecha/entorno | 2026-09-19; Windows x64; LLVM `C:\clang-llvm`; prefijo `A:\native-sharp` |
| Reporte Markdown | `selfhost/evidence/runs/tanda-02-sh01-final2-20260919/report.md`, SHA-256 `dbcfc854ef500e320f1a4792d330857fcd4f0bd7772f24135a57638116ef47dd` |
| Reporte JSON | `selfhost/evidence/runs/tanda-02-sh01-final2-20260919/report.json`, SHA-256 `6c9c7c721c4b2eb7ace4b41e9db1000b6cdeee0a6a41f19cdaffa4df83b0873f` |
| Reporte de perfil | `selfhost/evidence/gates/SH01/profile-report.json`, SHA-256 `c792b49585b7de6bd6dc7b3ffe2b65f243393c510f80b79fd69d2de0fbb10ada` |
| Estado | `PASSED`, 0 fallos, 453.759 s; S0 = S1 `7b7174c909b6e7a3f2ebff6ff8f7f98183617e174acf13a03a06cbf576bb0add` |

### Matriz y resultados esperados para el cierre

| Matriz | Total | Aprobadas | Fallidas | Omitidas |
|---|---:|---:|---:|---:|
| CTest/lld-link | 438 | 438 | 0 | 0 |
| CTest/NSLink | 438 | 438 | 0 | 0 |
| Fixtures (148 por linker) | 296 | 296 | 0 | 0 |
| Bug triage (18 × 2 linkers) | 36 | 36 | 0 | 0 |
| Self-hosting SH00/SH01 (3 × 2 linkers) | 6 | 6 | 0 | 0 |

### Corridas anteriores no promovidas

`tanda-02-sh01-final-20260919` pasó toda la matriz (438/438 por linker, 296/296 fixtures y 36/36
triage), pero al auditar sus artefactos se comprobó que el runner no emitía el `report.json` exigido
por el contrato: sólo `metadata.json` y `report.md`. La corrida se conserva, no se etiqueta como
fallida y tampoco se promueve como baseline de cierre. Se corrigió el runner y se ejecutó una nueva
corrida completa. Los smoke tests previos también encontraron y corrigieron que los campos de la
fixture `readonly struct` debían ser `readonly`; no constituyeron un baseline.

## 12. Riesgos, limitaciones y deuda

- el filtro no sustituye al parser/Sema y deliberadamente delega semántica completa a `nsc0`;
- la allowlist ejecutable cierra proyectos, mientras el inventario humano restringe la superficie
  de tipos; si aparece una evasión real se añade una regla con fixture en el mismo commit;
- el perfil inicial es single-thread; rendimiento paralelo se habilitará sólo con determinismo;
- las fronteras LLVM/Win32 son nombres reservados hasta su implementación en SH03.

## 13. Contenido del commit

### Archivos incluidos

El inventario final corresponde a las filas de la sección 4; antes de commit se verifican nombres
exactos con `git diff --cached --name-status` y se excluyen los cambios locales preexistentes.

### Higiene

- [x] No contiene `.obj`, `.exe`, `.dll`, `.lib`, PDB, temporales ni cachés no normativos.
- [x] No contiene cambios locales ajenos a la tanda.
- [x] El manifiesto de fuentes está actualizado cuando corresponde.
- [x] La documentación describe las firmas y archivos realmente incluidos.
- [x] El baseline se ejecutó después del último cambio funcional.

### Commit de cierre

```text
selfhost(tanda-02): freeze Bootstrap Profile 2.0
```

## 14. Criterio de cierre

- [x] `NS2-SH-010` a `NS2-SH-019` terminados.
- [x] Tests específicos verdes y fixture positiva compilada por `nsc0`.
- [x] Baseline completo verde con ambos linkers.
- [x] Gate SH01 `LANDED`, enlazado a SH00 y sin problemas.
- [x] Expediente, contrato y commit coinciden.
