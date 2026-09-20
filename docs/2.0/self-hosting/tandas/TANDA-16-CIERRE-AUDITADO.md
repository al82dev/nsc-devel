# NativeSharp 2.0 self-hosting — tanda 16: cierre auditado

## Identidad

| Campo | Valor |
|---|---|
| Estado | `CLOSED-WITH-EXPLICIT-BLOCKERS` |
| Entrada | `8f065e892` |
| Commit | `selfhost(tanda-16): seal audited closure and remaining-work ledger` |
| Alcance | tandas 7–16 |

## Qué cierra y qué no

Esta tanda cierra la ejecución metodológica solicitada: cada tanda tiene diseño, funciones,
archivos, gate o decisión, baseline y commit. Congela los commits 7–15, vuelve a leer todos los
gates SH06–SH14 y exige que el selector siga siendo seguro.

No declara NativeSharp autoalojado en sentido pleno. `release_promoted=false` y
`nsc0_retired=false` son resultados normativos. Dos trabajos quedan abiertos y bloquean la
promoción:

1. importar una atestación SH12 coincidente desde una segunda máquina Windows x64;
2. reemplazar `source-attested-image-replay` por codegen LLVM completo derivado de fuentes.

## Diseño, métodos y archivos

`closure.contract.json` fija las tandas 7–16, los gates requeridos, los commits conocidos y la
selección segura. `verify_closure.py` recalcula SHA-256 de cada gate, lee el selector SH14, exige
`default=nsc0` y `promotion_allowed=false`, y escribe:

- `tanda16-report.json`: acta técnica, estados, hashes, selección y dos pendientes;
- `remaining-work.json`: ledger mínimo con condición, archivos propietarios y gates bloqueados.

`VerifyNativeSharpSelfHostClosure.cmake` integra la auditoría en CTest. Una modificación que oculte
un gate, cambie silenciosamente el predeterminado o elimine un pendiente hace fallar el cierre.

## Historial congelado

| Tanda | Commit |
|---:|---|
| 7 | `e3bfbb3c8` |
| 8 | `58f7ecce2` |
| 9 | `ada216aca` |
| 10 | `72d181085` |
| 11 | `75dd06ed8` |
| 12 | `0e10d70fb` |
| 13 | `17084784a` |
| 14 | `e48b1ef1c` |
| 15 | `8f065e892` |
| 16 | este commit |

## Baseline final de la tanda

La corrida normativa es `tanda-16-selfhost-closure-certified-20260919`. Se ejecuta después de
cerrar este documento para que `source_digest` represente exactamente las fuentes del commit. Su
resultado autoritativo vive en `_abi/gates/G00/gate.json` y el informe completo en
`selfhost/evidence/runs/tanda-16-selfhost-closure-certified-20260919/report.json`.

Comando:

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work ^
  --temp-root A:\nsc-devel\selfhost\out\temp --write-root A:\ ^
  --run-id tanda-16-selfhost-closure-certified-20260919 ^
  --build-id 2000000000000000
```

Resultado exigido y certificado por `G00`:

| Comprobación | Resultado |
|---|---:|
| Estado global | `PASSED` |
| CTest `lld-link` | 452/452 |
| CTest `NSLink` | 452/452 |
| Fixtures por linker | 148/148 |
| Bug triage por linker | 18/18 |
| `nsc.assembly.abi-c-cpp-o0` | `PASSED` |
| `nsc.selfhost.tanda16-closure` | `PASSED` |
| Instalación verificada | `A:\native-sharp` |

La duración es una observación de máquina y se conserva en `report.json`; no es criterio de
aceptación. El compilador instalado usa la identidad reproducible
`2.0.0+build.19700101T000000Z.2000000000000000`.

## Evidencia versionada y resolución

- `selfhost/evidence/closure/tanda16-report.json`: estados y SHA-256 de SH06–SH14, selección segura
  y no promoción;
- `selfhost/evidence/closure/remaining-work.json`: los dos pendientes que siguen bloqueando la
  promoción;
- `_abi/gates/G00/gate.json`: identidad de fuentes, binarios, herramientas y corrida normativa.

El cierre es `CLOSED-WITH-EXPLICIT-BLOCKERS`: las tandas 7–16 quedan terminadas y auditadas, pero
NativeSharp todavía no satisface la definición estricta de autoalojamiento. `nsc0` permanece como
predeterminado y rollback; `nsc2` permanece candidato.
