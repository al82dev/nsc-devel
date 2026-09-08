# 147 — Baseline certificado de 1.0-E9 / Stage 63

**Fecha:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-190718-3fc07605`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## 1. Resultado

Stage 63 dejó de ser únicamente una implementación verde en prefijos de prueba. G00 reconstruyó
el árbol, instaló el ejecutable producido en `C:\native-sharp`, ejecutó las dos rutas de enlace y
certificó que el paquete instalado puede verificar su propio layout.

La promoción demuestra dos contratos distintos y complementarios:

- Stage 62: los componentes declaran una identidad compatible mediante un contrato determinista;
- Stage 63: el producto instalado contiene su superficie mínima y el `nsc.exe` instalado la acepta.

El segundo contrato no sustituye al primero. `--verify-installation` usa internamente
`verifyToolchainContract`, por lo que una instalación sólo es válida cuando existen sus piezas y
además pertenecen a la misma línea de toolchain.

## 2. Evidencia exacta

| Dato | Valor |
|---|---|
| Directorio de corrida | `A:\nsc-devel\_nscbuilder\history\20260907-190718-3fc07605` |
| Inicio | `2026-09-07 19:07:18 -06:00` |
| Fin | `2026-09-07 19:18:08 -06:00` |
| Duración | 650.010 s |
| Etapas operativas | 11/11 |
| Etapas totales con certificación | 12/12 |
| CTest lld-link | 301/301 |
| CTest NSLink | 301/301 |
| Casos registrados por G00 | 324/324 |
| Fallos | 0 |
| Archivos fuente medidos | 1024 al inicio y al final |

## 3. Inmovilidad del árbol

```text
S0 974924668e24d4307dc59d5e4250a535af48b9b08b642aca02f0989cccd7872a
S1 974924668e24d4307dc59d5e4250a535af48b9b08b642aca02f0989cccd7872a
```

El digest y el conteo de 1024 archivos coincidieron antes de construir y después de ejecutar. No
hubo una edición tardía de documentación, código o fixture entre las etapas: toda la certificación
midió la misma fotografía de Stage 63.

## 4. Identidad construida e instalada

```text
built_nsc     859097252e07d479e3d6b85aadd9e8ab5fb83b43df19f70e2a7c7ec3c4a41be7
installed_nsc 859097252e07d479e3d6b85aadd9e8ab5fb83b43df19f70e2a7c7ec3c4a41be7
```

La igualdad prueba que G00 no ejecutó un compilador anterior ni una copia distinta de la que acababa
de construir.

Identidad visible instalada:

```text
nsc 0.9.0+build.20260908T010719Z.90e9477a1f3fdd78
NativeSharp 0.1, stage 63
LLVM 23.1.0 (supported: 23.x)
```

La fecha del build id está expresada en UTC; la corrida G00 está registrada con la zona local
`-06:00`. Por eso la identidad muestra 2026-09-08 aunque el acta local pertenece al 7 de septiembre.

## 5. Contrato e instalación oficiales

Contrato instalado:

```text
C:\native-sharp\share\nativesharp\toolchain\NativeSharpToolchain.contract
```

SHA-256:

```text
631FE550BA132985F59CC6CDBB29C2EDBC4CBF143242C8F9ABD1FC6509792B00
```

La verificación explícita posterior contra el producto promovido terminó así:

```text
NativeSharp installation verified: C:/native-sharp
```

El contrato tiene el mismo hash que el prefijo aislado de cierre técnico. Esto es lo esperado:
ambos son Stage 63 compatibles aunque sus `nsc.exe` tengan build id y hash binario diferentes.

## 6. Qué recorrió G00

Las etapas operativas fueron:

1. construir `nsc`;
2. instalar el producto;
3. construir CTest para lld-link;
4. construir CTest para NSLink;
5. sincronizar las bibliotecas distribuidas;
6. ejecutar CTest, fixtures y bug triage con lld-link;
7. ejecutar CTest, fixtures y bug triage con NSLink;
8. certificar la fotografía inicial como baseline.

La etapa de instalación incorporó la nueva barrera de Stage 63. CMake sólo pudo marcarla como
`passed` después de que el ejecutable ubicado en `C:\native-sharp\bin\nsc.exe` validara prefijo,
contrato, manifiestos Core y especificaciones.

## 7. Dos contadores que no deben confundirse

El reporte presenta dos universos de evidencia:

- **301 pruebas CTest por linker:** suites unitarias, contractuales e integraciones registradas en
  CMake. Aquí aparece `nsc.stage63.installation-verifier` con sus 25 checks internos.
- **324 casos G00:** ejecuciones que el runner clasifica por linker y optimización, incluidas
  fixtures y triage. Se reparten exactamente en 81 casos para cada cuadrante.

| Linker | Optimización | Casos | Fallos |
|---|---:|---:|---:|
| lld-link | O0 | 81 | 0 |
| lld-link | O2 | 81 | 0 |
| NSLink | O0 | 81 | 0 |
| NSLink | O2 | 81 | 0 |

Por tanto, `301/301` y `324/324` no son cifras rivales ni una pérdida de cobertura. Miden capas
distintas del mismo gate.

## 8. Qué quedó promovido

El baseline oficial ahora garantiza que:

- `nsc --verify-installation <prefix>` forma parte del ejecutable instalado;
- una omisión de la superficie mínima detiene la instalación;
- el contrato Stage 63 se instala con bytes deterministas;
- CMake usa el prefijo efectivo y el ejecutable recién copiado;
- la guía de construcción corresponde a Windows x64 y LLVM 23.x;
- ambos linkers conservan la matriz completa en verde;
- el árbol y el producto permanecen identificados por hashes verificables.

## 9. Qué no afirma este baseline

La promoción comprueba la superficie mínima, no todavía todos los bytes del paquete. Un archivo
interno de Core que no sea uno de los manifiestos exigidos será cubierto por el inventario completo
de la siguiente etapa.

Tampoco compara dos builds independientes ni emite todavía información nativa de fichero y línea.
Esos son trabajos posteriores y no se infieren de que `cmake --install` haya pasado.

## 10. Estado de cierre y siguiente paso

Stage 63 está construido, instalado, `LANDED` y certificado. El baseline oficial deja de ser Stage
62.

El siguiente bloque de E9 es Stage 64: producir un inventario determinista de cada archivo público
instalado, con ruta normalizada, tamaño y SHA-256; instalarlo; verificarlo desde `nsc`; y detectar
archivos faltantes, alterados o inesperados con diagnósticos precisos.
