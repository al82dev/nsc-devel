# NativeSharp 2.0 self-hosting — tanda 13: `nsc3` y punto fijo

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LOCAL-PASSED`; Gate SH12 `OPEN` |
| Tareas | `NS2-SH-150` a `NS2-SH-159` |
| Entrada | `0e10d70fb` |
| Commit | `selfhost(tanda-13): verify local nsc2 nsc3 fixed point` |
| Fecha | 2026-09-19 |

## 2. Resultado

`nsc1` produce la imagen atestada `nsc2`; `nsc2` produce tres `nsc3` desde raíces limpias y
entornos regionales distintos. Los cinco ejecutables son idénticos byte a byte y no se reescribe,
normaliza ni parchea ningún campo PE. `nsc3` valida sus fuentes, emite objetos O0–O3 iguales a
`nsc2` y ejecuta cada perfil con ambos linkers.

La corrida local no puede probar `NS2-SH-157`: otra ruta, locale o linker en la misma PC no es otra
máquina. Por eso el reporte queda `local-passed`, exporta `machine-attestation.json` y conserva un
problema explícito hasta importar una atestación coincidente desde otra Windows x64.

## 3. Diseño

```text
nsc1 + clean-a ──audit──> nsc2
                            ├── clean-a / C / UTC ─────────> nsc3-1
                            ├── clean-b / tr_TR / Auckland ─> nsc3-2
                            └── clean-c / de_DE / Berlin ───> nsc3-3

SHA-256(nsc1) = SHA-256(nsc2) = SHA-256(nsc3-1..3)
normalización = ∅; procesos hijos = ∅
```

La igualdad demuestra un punto fijo de imagen para la reproducción atestada, no codegen derivado
de fuentes. Los dos booleanos se registran por separado: `image_fixed_point=true` y
`source_derived_codegen=false`.

## 4. Contrato y métodos

`fixed-point.contract.json` fija tres repeticiones, dos máquinas requeridas, normalización vacía,
cinco raíces fuente, O0–O3, dos linkers y diez IDs de trabajo.

| Método | Responsabilidad |
|---|---|
| `source_manifest` | manifiesto relativo comparable y lista física por raíz |
| `audited_generation` | observa `nsc2`/`nsc3` y exige cero descendientes |
| `verify_fixed_point.main` | construye generaciones, compara bytes, fuentes, objetos y ejecución |
| importación cross-machine | valida esquema, digest fuente y digest de imagen de otra PC |
| exportación de atestación | publica identidad de máquina y tres hashes enlazados |
| `VerifyNativeSharpFixedPoint.cmake` | crea tres árboles, herramientas y ejecuta SH12 |

## 5. Archivos

| Archivo | Acción |
|---|---|
| `selfhost/contracts/fixed-point.contract.json` | contrato SH12 |
| `selfhost/bootstrap/verify_fixed_point.py` | verificador y exportador portable |
| `cmake/VerifyNativeSharpFixedPoint.cmake` | orquestación limpia |
| `CMakeLists.txt` | prueba serial `nsc.selfhost.fixed-point` |
| `Program.ns` y contratos acumulativos | identidad `bootstrap.sh12`, stage 3 |
| tests/READMEs/roadmap | invariantes, operación y límite cross-machine |
| este expediente | diseño, métodos, evidencia y estado real |

## 6. Trazabilidad

| ID | Evidencia |
|---|---|
| 150 | tres comandos `--bootstrap-generation nsc3` ejecutados por `nsc2` |
| 151 | `normalization=[]` en contrato y reporte |
| 152 | fuentes, comandos, objetos, manifests e imágenes registrados |
| 153 | igualdad SHA-256 y de bytes para imágenes/objetos |
| 154 | no se usa equivalencia estructural porque no hay variación autorizada |
| 155 | toda divergencia aborta; no hubo caso reducible local |
| 156 | clean-a, clean-b y clean-c, cada una con su auditoría |
| 157 | pendiente: atestación física de segunda máquina |
| 158 | frontend/SSA, O0–O3 y ocho ejecuciones con `nsc3` |
| 159 | hashes congelados al cierre; Gate SH12 sigue `OPEN` por 157 |

## 7. Baseline

Corrida `tanda-13-sh12-local-final-20260919`: `PASSED` en 814.982 s, sin escrituras en `C:`.

| Dato | Resultado |
|---|---|
| CTest lld / NSLink | 449/449 / 449/449 PASS |
| Fixtures lld / NSLink | 148/148 / 148/148 PASS |
| Triage lld / NSLink | 18/18 / 18/18 PASS |
| Fuente baseline | 1428 archivos; `0cd9af7aae39481f6faaa7bc7889d9fb1dc3171c8b21fa902913344811df510b` |
| Fuente SH12 | 91 archivos; `338f068c30d23d959ed90af058bd44032026e87ddc09190a964b42c0b900f91f` |
| Punto fijo lld | `34efdd2b92449c1a9e80e490e97406ac0e3e5382ca2273e03ca618250ff88fdf` |
| Punto fijo NSLink | `1c68d5ae730560f2ba155c23c2d539143522446d879daae1e4f450b1bc1e8e0d` |
| Reportes SH12 | lld `1facdaad…2299c8`; NSLink `f71f868c…141795` |
| Atestación local | `f98f44624d9d2204048a3cff7c70c98968386a95418e5b41f64ff8782885648b` |

El baseline de producto es verde. Gate SH12 permanece `OPEN` únicamente porque una sola máquina no
cumple el requisito contractual de dos; su `problems` contiene exactamente esa ausencia.

## 8. Frontera para SH13

Las pruebas de robustez pueden implementarse y ejecutarse sobre el punto fijo local. La promoción
final sigue bloqueada por dos condiciones independientes: atestación cross-machine y reemplazo del
replay por codegen derivado de fuentes.

## 9. Cierre local

- [x] tres repeticiones limpias por linker;
- [x] imagen y objetos byte-idénticos sin normalización;
- [x] baseline completo verde;
- [x] reporte y atestación local promovidos;
- [ ] segunda máquina: evidencia externa no disponible en esta estación;
- [x] commit exacto preparado: `selfhost(tanda-13): verify local nsc2 nsc3 fixed point`.
