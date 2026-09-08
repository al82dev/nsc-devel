# 154 — Baseline certificado de 1.0-E11 / Stage 67

**Fecha:** 2026-09-08

**Compilador:** `nsc 0.9.0`

**Lenguaje:** NativeSharp 0.1

**Fotografía:** Stage 67

**Estado:** `LANDED`, certificado

## 1. Resultado

E11 dejó de ser únicamente una implementación local. G00 reconstruyó el compilador desde el árbol
de fuentes, lo instaló en `C:\native-sharp`, levantó las dos matrices de CTest, ejecutó las fixtures
y el bug triage en O0/O2 con ambos enlazadores, y finalmente escribió un baseline certificado.

```text
run_id:     20260908-092439-7d0bd312
inicio:     2026-09-08 09:24:39 -06:00
fin:        2026-09-08 09:37:19 -06:00
duración:   760.460 s
estado:     LANDED
certified:  true
fallos:     0
```

La evidencia canónica está en:

- `_abi/gates/G00/gate.json`;
- `_nscbuilder/history/20260908-092439-7d0bd312/metadata.json`;
- `_nscbuilder/history/20260908-092439-7d0bd312/report.md`;
- los logs y casos conservados debajo del mismo directorio de corrida.

## 2. Inmovilidad del árbol

G00 midió el árbol antes de construir y después de terminar toda la corrida:

```text
archivos S0: 1035
archivos S1: 1035
S0: 7200c74938f0469350409444a4f327e18b093af9df9b958e041920a3649fd8a9
S1: 7200c74938f0469350409444a4f327e18b093af9df9b958e041920a3649fd8a9
```

La igualdad prueba que las pruebas observaron la misma fotografía que se construyó. Este documento
se agrega después del `LANDED`, como acta histórica; por ello no se presenta como parte retroactiva
de aquellos 1,035 archivos ni altera los hashes registrados por el gate.

## 3. Identidad construida e instalada

El ejecutable instalado se identifica así:

```text
nsc 0.9.0+build.20260908T152440Z.079cf744116ec119
(NativeSharp 0.1, stage 67)
build 079cf744116ec119
```

La copia construida y la instalada son byte por byte el mismo ejecutable:

```text
built_nsc:     f7e920496191650c460ff580ed49d05e0cbefb6356d685f2a7784c28b65668e6
installed_nsc: f7e920496191650c460ff580ed49d05e0cbefb6356d685f2a7784c28b65668e6
tamaño:        4,045,312 bytes
```

Esto enlaza tres identidades distintas, sin confundirlas:

- `0.9.0`: compatibilidad pública del compilador;
- Stage 67: fotografía funcional que contiene E11;
- `079cf744116ec119`: identidad del build instalado concreto.

## 4. Las doce etapas

| Etapa | Resultado | Tiempo |
|---|---:|---:|
| Construir | passed | 10.826 s |
| Instalar | passed | 0.313 s |
| Construir CTest con lld-link | passed | 45.433 s |
| Construir CTest con NSLink | passed | 44.347 s |
| Sincronizar bibliotecas | passed | 0.024 s |
| CTest con lld-link | passed | 139.864 s |
| Fixtures con lld-link | passed | 163.841 s |
| Bug triage con lld-link | passed | 28.881 s |
| CTest con NSLink | passed | 128.891 s |
| Fixtures con NSLink | passed | 164.247 s |
| Bug triage con NSLink | passed | 28.206 s |
| Certificar baseline G00 | passed | 2.606 s |

No hubo problemas registrados y todas las etapas terminaron con código de salida cero.

## 5. Matriz CTest

Cada configuración reconstruyó y ejecutó 304 pruebas:

```text
lld-link: 304/304 passed
NSLink:   304/304 passed
```

La nueva prueba `nsc.stage67.e11-lambda-execution` aparece expresamente en ambas matrices. Esto es
importante: E11 no quedó validada sólo por inspección de HIR/MIR/LLVM. El programa resultante se
ejecutó en O0 y O2 y comprobó la semántica compartida de las clausuras.

## 6. Runner de programas

El runner registró 324 casos aprobados:

| Enlazador | Casos | Resultado |
|---|---:|---:|
| lld-link | 162 | 162 passed |
| NSLink | 162 | 162 passed |
| **Total** | **324** | **324 passed** |

Cada familia incluye las dos optimizaciones certificadas, O0 y O2. Además de las fixtures del
lenguaje, la corrida conservó bug triage, ABI, ownership, excepciones, genéricos, arrays y llamadas
nativas. Así se verificó que el cambio de representación de capturas no desplazó contratos ya
cerrados.

## 7. Contrato instalado

`NativeSharpToolchain.contract` publica:

```text
compiler_release=0.9.0
language=0.1
stage=67
target=x86_64-pc-windows-msvc
llvm=23.x
nsmeta=1.8
native_abi=1.0
runtime_abi=1.0
managed_delegate_cell_size=32
supported_linkers=lld-link,NSLink
certified_optimizations=O0,O2
artifact_kinds=exe,static-lib,dll
```

E11 cambia cómo una closure administra su entorno, pero no cambia el tamaño congelado de la celda
de delegate administrado ni las versiones de ABI. La celda compartida de captura vive detrás del
puntero de contexto y por eso no rompe el contrato binario publicado en Stage 66.

## 8. Inventario criptográfico instalado

El manifiesto `NSINSTALL 1` contiene una cabecera y 77 entradas autenticadas. Los artefactos
centrales quedaron así:

| Artefacto | Bytes | SHA-256 |
|---|---:|---|
| `bin/nsc.exe` | 4,045,312 | `f7e920496191650c460ff580ed49d05e0cbefb6356d685f2a7784c28b65668e6` |
| `NativeSharpCompatibility.policy` | 410 | `2c91b836333efe84a00b2d6127f5ede42f9bdd3078389d21b7270e57d54636e4` |
| `NativeSharpInstallation.files` | 3,629 | `98e2324fcf2a8f1875fb9ab1163c6dd3dc42f268226db7bd397b1d77e38caaa6` |
| `NativeSharpInstallation.manifest` | 8,956 | `d7601665c3f75eb390da79f379eecaa3de66cd163f534f78e0dea89ebfa80757` |
| `NativeSharpToolchain.contract` | 435 | `6203260ba3fffc9f9ca61157296da1d9d46643b08fc9e06d4b3ffb9c7b4b30af` |

El hash de `nsc.exe` dentro del manifiesto coincide con `built_nsc`, `installed_nsc` y el archivo
físico leído después de la instalación.

## 9. Qué queda certificado de E11

Desde esta corrida se puede afirmar conjuntamente que:

- las capturas ordinarias comparten una celda mutable;
- las lambdas hermanas observan las mismas escrituras;
- las lambdas anidadas transportan capturas transitivas;
- los parámetros explícitos y por referencia llegan a ejecución;
- structs y fixed arrays mayores de ocho bytes conservan su valor y alineación;
- el conteo de referencias libera entornos y celdas en salidas normales y excepcionales;
- un `catch` local no destruye antes de tiempo la celda de la activación;
- una lambda sin captura cruza como callback C y una captura incompatible se diagnostica;
- ambos enlazadores y ambos niveles de optimización preservan el resultado.

## 10. Promoción

Stage 67 sustituye a Stage 66 como baseline certificado vigente. Stage 66 continúa siendo el acta
histórica del cierre de E9+E10; sus contratos de paquete y compatibilidad permanecen activos y
fueron revalidados en esta corrida.

E11 queda cerrada: implementada, documentada, construida, instalada, ejecutada y certificada.
