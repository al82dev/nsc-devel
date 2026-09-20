# NativeSharp 2.0 self-hosting — tanda 17: retiro de image replay

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | retirar reproducción de imagen; activar fallo cerrado; auditar ausencia de PE |
| Gate | `SH15` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` |
| Commit de entrada | `3f7fdc961` |
| Commit de cierre | este commit (`selfhost(tanda-17): retire bootstrap image replay`) |
| Fecha de apertura/cierre | 2026-09-20 |
| Baseline de cierre | `tanda-17-sh15-image-replay-retired-20260920` |

## 2. Objetivo y alcance

La tanda elimina el mecanismo que hacía pasar una copia de `nsc1.exe` por una nueva generación.
Desde este commit no se puede materializar `nsc2` hasta que el backend NativeSharp compile las
fuentes reales del compilador. La solicitud sigue validando el manifiesto para mantener auditable
la frontera, escribe la causa del bloqueo y termina sin PE.

Incluye el contrato SH15, el verificador Python, su integración CTest, una prueba estructural y la
retirada de las tres pruebas activas cuya premisa era la reproducción de imagen. No implementa el
codegen completo del compilador, no reescribe evidencia histórica y no promueve `nsc2`.

## 3. Diseño

### Arquitectura y flujo

`nsc0` construye `nsc1` desde `NativeSharp.Compiler.nsproj`. El test invoca a `nsc1` con
`--bootstrap-generation`, un manifiesto de las fuentes `.ns`, la ruta del productor, una salida
prohibida y una traza. `GenerationBuilder.Build` verifica argumentos y manifiesto, escribe
`method=source-derived-codegen-required`, comprueba que no exista salida y retorna 78. El verificador
comprueba además los hashes del productor y los fragmentos prohibidos del código fuente.

### Contratos e invariantes

- `image_replay_allowed=false` y `source_derived_codegen=false`.
- El código 78 significa “generación bloqueada por backend incompleto”.
- Una solicitud bloqueada jamás crea, reemplaza ni conserva el PE de salida.
- El productor es inmutable durante la operación.
- SH15 `LANDED` sólo certifica el retiro del atajo; SH12–SH14 continúan `OPEN`.
- La evidencia histórica conserva su significado y no vuelve a entrar en la matriz activa.

| Decisión | Alternativas | Razón |
|---|---|---|
| Fallar cerrado | copiar, emitir el programa canónico 42, omitir la prueba | evita una certificación falsa y deja un error reproducible |
| Código 78 | código genérico 1 | distingue una capacidad temporalmente no disponible de entrada corrupta |
| Gate nuevo SH15 | reescribir SH11 | conserva inmutable el expediente histórico |

## 4. Inventario de archivos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/Bootstrap/GenerationBuilder.ns` | modificar | bloquear generación sin codegen y retirar la copia |
| `selfhost/contracts/image-replay-retirement.contract.json` | crear | contrato ejecutable SH15 |
| `selfhost/bootstrap/verify_image_replay_retired.py` | crear | prueba dinámica y estructural del retiro |
| `cmake/VerifyNativeSharpImageReplayRetired.cmake` | crear | construir `nsc1` y ejecutar el verificador |
| `CMakeLists.txt` | modificar | sustituir las pruebas basadas en replay por SH15 |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | regresión estática del contrato y la implementación |
| `docs/2.0/SELF_HOSTING_ROADMAP.md` | modificar | registrar la reapertura técnica |
| `selfhost/README.md` | modificar | publicar la frontera real alcanzada |
| `selfhost/bootstrap/README.md` | modificar | documentar el verificador |
| `selfhost/contracts/README.md` | modificar | documentar el contrato |

## 5. Tipos y datos

No se crean tipos NativeSharp. El contrato JSON versión 1 define `blocked_exit_code`, mínimos del
conjunto fuente, campos obligatorios de traza y fragmentos de implementación prohibidos. El reporte
`nativesharp-selfhost-sh15-report` versión 1 conserva estado, hashes, conteo de fuentes, salidas y
problemas.

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas/salida | Errores y efectos |
|---|---|---|---|
| `GenerationBuilder.Build(args: string[]): int` | validar una solicitud de generación y bloquearla hasta disponer de codegen | seis argumentos; 78 tras escribir traza; 1 si la solicitud es inválida | escribe sólo la traza; no crea PE ni modifica productor; single-thread |
| `verify_image_replay_retired.main(): int` | ejecutar y auditar la frontera SH15 | contrato, raíz, `nsc1`, salida y reporte; 0 si se demuestra el retiro | crea manifest/traza/reporte dentro de `out`; 1 ante divergencia |
| `verify_image_replay_retired.digest(path): str` | calcular SHA-256 del productor | archivo existente; hexadecimal | lectura sin mutación |
| `verify_image_replay_retired.require(condition, message)` | abortar en la primera infracción | predicado y mensaje | levanta `VerificationError` |

## 7. Superficie observable

No cambia la sintaxis de NativeSharp. Cambia deliberadamente
`nsc1 --bootstrap-generation nsc2 MANIFEST PRODUCER OUTPUT TRACE`: antes podía devolver 0 copiando
`PRODUCER`; ahora devuelve 78, emite el diagnóstico `image replay retired`, escribe una traza v1 y
garantiza la ausencia de `OUTPUT`. No se añade variable de entorno ni dependencia externa.

## 8. Implementación

Primero se retiró `File.Copy`. Después se fijó el contrato de fallo cerrado y se construyó el
verificador que prueba la imagen compilada, no sólo el texto. Finalmente se retiraron de CTest las
pruebas de generación, punto fijo y robustez cuya precondición era el replay. Sus archivos y
evidencia permanecen para trazabilidad, pero no producen aprobación activa.

## 9. Pruebas

| Prueba | Tipo | Propósito | Resultado/evidencia |
|---|---|---|---|
| `test_tanda17_retires_image_replay_and_fails_closed` | regresión estática | contrato, ausencia de copia, traza y código 78 | debe aprobar |
| `nsc.selfhost.image-replay-retired` con `lld-link` | negativa/dinámica | construir `nsc1` y demostrar que no genera PE | `selfhost/evidence/gates/SH15/sh15-lld-report.json` |
| `nsc.selfhost.image-replay-retired` con NSLink | negativa/dinámica | repetir con el enlazador hermano | `selfhost/evidence/gates/SH15/sh15-nslink-report.json` |
| baseline completo | regresión acumulativa | proteger compilador, instalación, fixtures y triage | G00 y reporte de corrida |

## 10. Comparación diferencial

La diferencia respecto de la generación anterior es intencional: el resultado cambia de una copia
byte a byte con código 0 a ausencia de salida con código 78. No se autoriza normalización. Esta
diferencia corrige la clasificación de capacidad; no afirma equivalencia con `nsc0`.

## 11. Baseline de cierre

Comando normativo:

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work ^
  --temp-root A:\nsc-devel\selfhost\out\temp --write-root A:\ ^
  --run-id tanda-17-sh15-image-replay-retired-20260920 ^
  --build-id 2000000000000000
```

El resultado autoritativo se obtiene después del último cambio funcional y vive en
`_abi/gates/G00/gate.json`; el informe completo vive en
`selfhost/evidence/runs/tanda-17-sh15-image-replay-retired-20260920/report.json`. La matriz esperada
es 450/450 CTest por enlazador, 148/148 fixtures y 18/18 triage por enlazador, incluyendo SH15.
Una corrida fallida no puede sustituir esta evidencia.

## 12. Riesgos, limitaciones y deuda

- El compilador NativeSharp todavía no puede compilar su propia implementación.
- SH12 carece de una segunda máquina y su punto fijo anterior dependía del mecanismo retirado.
- La funcionalidad aparente disminuye hasta que las siguientes tandas implementen codegen real.
- El código C++ `nsc0` sigue siendo compilador predeterminado y ruta de recuperación.

## 13. Contenido e higiene del commit

El commit contiene únicamente los once archivos enumerados, G00 y evidencia SH15. No incorpora
objetos, ejecutables, bibliotecas, PDB, temporales, cachés ni cambios locales ajenos. El manifiesto
fuente no cambia porque descubre los `.ns` dinámicamente. El baseline se ejecuta después de este
documento y del último cambio funcional.

Commit: `selfhost(tanda-17): retire bootstrap image replay`.

## 14. Criterio de cierre

- [x] El replay está retirado del código y de la matriz activa.
- [x] La prueba específica demuestra fallo cerrado y ausencia de salida.
- [x] El baseline completo queda ligado a G00.
- [x] SH15 tiene evidencia con problemas vacíos.
- [x] El expediente no confunde retiro del atajo con autocompilación.
