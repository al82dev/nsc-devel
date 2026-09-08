# 145 — Baseline certificado de 1.0-E9 / Stage 62

**Fecha:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-175537-29408b6e`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## Resultado

La primera megaentrega E9 dejó de ser una implementación validada únicamente en prefijo aislado.
G00 reconstruyó Stage 62, instaló exactamente ese ejecutable en `C:\native-sharp`, recorrió las
dos rutas de enlace y certificó el contrato reproducible del toolchain sin mutaciones del árbol.

El producto oficial ahora incluye `NativeSharpToolchain.contract` y el propio compilador instalado
lo acepta. Esto cierra la diferencia que antes existía entre «el código sabe producir el
manifiesto» y «el paquete certificado realmente lo transporta».

## Evidencia exacta

| Dato | Valor |
|---|---|
| Directorio de corrida | `A:\nsc-devel\_nscbuilder\history\20260907-175537-29408b6e` |
| Inicio | `2026-09-07 17:55:37 -06:00` |
| Fin | `2026-09-07 18:06:15 -06:00` |
| Duración | 637.655 s |
| Etapas operativas | 11/11 |
| Etapas totales con certificación | 12/12 |
| Casos | 324/324 |
| Fallos | 0 |
| Archivos fuente medidos | 1021 al inicio y al final |

## Inmovilidad del árbol

```text
S0 2878b5a58dff7974a4633ded4b861dd931c36d1fcc944fcfcec01a76919929ba
S1 2878b5a58dff7974a4633ded4b861dd931c36d1fcc944fcfcec01a76919929ba
```

La igualdad no es aproximada: el digest y el conteo de 1021 archivos coincidieron. Por tanto, la
compilación, instalación y ejecución midieron una sola fotografía del código.

## Identidad construida e instalada

```text
built_nsc     25c68afdd56508e54e886e2b66a1d6d44ae93db49b3349de89d46cc220b1a45d
installed_nsc 25c68afdd56508e54e886e2b66a1d6d44ae93db49b3349de89d46cc220b1a45d
```

La identidad visible del producto es:

```text
nsc 0.9.0+build.20260907T235538Z.5f4adfd699784d25
NativeSharp 0.1, stage 62
LLVM 23.1.0 (supported: 23.x)
```

## Contrato instalado

Ruta oficial:

```text
C:\native-sharp\share\nativesharp\toolchain\NativeSharpToolchain.contract
```

SHA-256:

```text
23009776C7B35C42D0515E5E588D0CD4C80E25C1E463FCC4FF9BF52612128D18
```

La orden ejecutada contra el producto promovido terminó correctamente:

```text
nsc --verify-toolchain-contract NativeSharpToolchain.contract
toolchain contract verified
```

Así quedan conectadas las tres identidades que E9 necesitaba: valores compilados dentro de `nsc`,
manifiesto producido desde esos valores y copia instalada dentro del paquete certificado.

## Qué recorrió G00

Las doce etapas pasaron:

1. construir e instalar `nsc`;
2. construir las dos baterías CTest;
3. sincronizar las bibliotecas distribuidas;
4. CTest, fixtures y bug triage con `lld-link`;
5. CTest, fixtures y bug triage con NSLink;
6. certificar el baseline contra la fotografía inicial.

Los 324 casos cubrieron O0/O2, EXE, biblioteca estática, DLL, consumo entre proyectos, hosts,
ownership, excepciones, genéricos, ABI y las suites normativas E7–E9.

## Lectura del resultado

G00 no demuestra todavía reproducibilidad byte por byte entre dos máquinas independientes. Sí
demuestra algo previo e indispensable: una construcción concreta conserva el árbol inmóvil,
instala exactamente el ejecutable construido y transporta un contrato determinista que el mismo
ejecutable puede verificar.

## Estado de cierre

Stage 62 está promovido, instalado y certificado. El baseline oficial deja de ser Stage 61. E9
puede continuar sobre esta base con construcción limpia, perfiles Debug/Release, símbolos nativos
y control explícito de actualización/desinstalación.
