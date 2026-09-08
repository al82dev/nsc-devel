# 151 — Baseline certificado de 1.0-E9+E10 / Stage 66

**Fecha local:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-233625-801b209e`  
**Compilador:** `nsc 0.9.0`  
**Lenguaje:** NativeSharp 0.1  
**Fotografía:** Stage 66  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## 1. Qué significa esta promoción

Stage 66 dejó de ser un cierre técnico local. G00 volvió a construir `nsc.exe`, instaló esa
construcción en `C:\native-sharp`, reconstruyó las dos matrices de pruebas y ejecutó el corpus en
los cuatro cuadrantes de linker y optimización.

Con esta corrida quedaron promovidas juntas:

- E9: reproducción de paquetes, propiedad criptográfica, actualización y retiro;
- E10: política versionada y ejecutable de compatibilidad.

`gate.json` conserva la fotografía de fuentes, los hashes del compilador, el resultado de cada
etapa, los casos ejecutados y la decisión final. La certificación no se infiere de un log verde.

## 2. Resultado exacto

| Dato | Valor |
|---|---|
| Directorio | `A:\nsc-devel\_nscbuilder\history\20260907-233625-801b209e` |
| Inicio | `2026-09-07 23:36:25 -06:00` |
| Fin | `2026-09-07 23:46:49 -06:00` |
| Duración | 623.734 s |
| Etapas operativas | 11/11 |
| CTest lld-link | 303/303 en 123.87 s |
| CTest NSLink | 303/303 en 120.94 s |
| Casos del runner | 324/324 |
| Casos omitidos | 0 |
| Problemas registrados | 0 |
| Archivos fuente medidos | 1034 al inicio y al final |

Las once etapas construyeron e instalaron el compilador, construyeron ambas configuraciones CTest,
sincronizaron las bibliotecas y ejecutaron CTest, fixtures y bug triage primero con lld-link y luego
con NSLink. Todas terminaron con exit code 0.

## 3. Inmovilidad de la fotografía

```text
S0 55c96bd3b0f86885466689e1723d9aa13235a171cdd59de7fb75f4f08a8bb665
S1 55c96bd3b0f86885466689e1723d9aa13235a171cdd59de7fb75f4f08a8bb665
```

El digest y el conteo coinciden antes de construir y después de terminar la matriz. Construcción,
instalación y ejecución pertenecen a una sola fotografía de 1034 archivos.

## 4. Compilador promovido

```text
built_nsc     9958ba8ab401ada17879b412670770b4504adb32b8ec9f3cee2fc6d7783d63b1
installed_nsc 9958ba8ab401ada17879b412670770b4504adb32b8ec9f3cee2fc6d7783d63b1
```

La igualdad prueba que el ejecutable usado desde el prefijo oficial es exactamente el producido
por G00. Su identidad visible es:

```text
nsc 0.9.0+build.20260908T053626Z.eefaf302df6e31c8 (NativeSharp 0.1, stage 66)
build eefaf302df6e31c8
```

El build id usa UTC; por eso contiene 2026-09-08 aunque la corrida comenzó el día 7 en la zona
local `-06:00`.

## 5. Cuatro cuadrantes completos

| Linker | Optimización | Casos | Aprobados | Fallos |
|---|---:|---:|---:|---:|
| lld-link | O0 | 81 | 81 | 0 |
| lld-link | O2 | 81 | 81 | 0 |
| NSLink | O0 | 81 | 81 | 0 |
| NSLink | O2 | 81 | 81 | 0 |

La misma instalación produjo y ejecutó programas con ambos linkers y con semántica O0/O2. E9 y
E10 no quedaron certificadas solamente por sus pruebas unitarias.

## 6. Paquete instalado promovido

Stage 66 instala 77 entradas autenticadas. Sus cuatro autoridades centrales quedaron así:

| Artefacto | Formato | Entradas | Bytes | SHA-256 |
|---|---|---:|---:|---|
| `NativeSharpInstallation.files` | `NSFILES 1` | 77 | 3629 | `98e2324fcf2a8f1875fb9ab1163c6dd3dc42f268226db7bd397b1d77e38caaa6` |
| `NativeSharpInstallation.manifest` | `NSINSTALL 1` | 77 | 8956 | `873c13bc83847d51a8feed6e743cb099b0c68988595df0b937a586a5f9e51ac3` |
| `NativeSharpToolchain.contract` | `NSTOOLCHAIN 1` | — | 435 | `95484f1c42dbcc0bb3eddd1c49ad5e886801ae7b250e3b709a8f645cafe64ca6` |
| `NativeSharpCompatibility.policy` | `NSCOMPAT 1` | — | 410 | `2c91b836333efe84a00b2d6127f5ede42f9bdd3078389d21b7270e57d54636e4` |

El manifiesto autentica la política de compatibilidad igual que autentica Core, bibliotecas,
contratos y especificación.

## 7. Qué añade al baseline anterior

Stage 64 demostraba qué bytes poseía la instalación. Stage 66 conserva eso y agrega:

- comparación de dos construcciones limpias mediante `NSREPRO 1`;
- PE reproducible sin timestamp accidental de enlace;
- comparación del `nsc.exe` completo con identidad reservada para el gate;
- planes `NSUPDATE 1` ligados a los manifiestos anterior y siguiente;
- reconstrucción canónica y preflight completo antes de retirar archivos;
- preservación de archivos modificados y de vecinos ajenos al paquete;
- retiro completo sin borrar las raíces compartidas;
- política `NSCOMPAT 1` generada por una autoridad única;
- decisión ejecutable de compatibilidad `major.minor.patch`;
- versiones separadas para metadata, ABI y formatos del paquete.

## 8. Cadena de confianza

```text
fuentes inmóviles
    -> nsc.exe construido
    -> nsc.exe instalado idéntico
    -> NSFILES 1 define el payload
    -> NSINSTALL 1 autentica sus bytes
    -> NSREPRO 1 compara builds independientes
    -> NSUPDATE 1 limita qué puede retirarse
    -> NSCOMPAT 1 declara qué versiones entiende el producto
```

Cada flecha tiene implementación, comando público y prueba. El resultado no depende de memoria
humana para saber qué copiar, qué retirar o qué combinación de versiones aceptar.

## 9. Evidencia conservada

- `A:\nsc-devel\_abi\gates\G00\gate.json`;
- `A:\nsc-devel\_nscbuilder\history\20260907-233625-801b209e\metadata.json`;
- `A:\nsc-devel\_nscbuilder\history\20260907-233625-801b209e\report.md`;
- logs individuales dentro de `stages`;
- evidencia separada de lld-link y NSLink.

La autoridad final registra `problems: []`, `certified: true` y `state: LANDED`.

## 10. Estado después del acta

E9 y E10 están implementadas, instaladas y certificadas en Stage 66. Stage 64 deja de ser el
baseline oficial vigente. No queda una subentrega pendiente dentro de E9 o E10.

Cualquier modificación posterior abre una fotografía nueva y deberá obtener su propio G00; no
altera retroactivamente esta acta.
