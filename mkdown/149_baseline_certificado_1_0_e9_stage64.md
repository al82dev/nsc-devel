# 149 — Baseline certificado de 1.0-E9 / Stage 64

**Fecha:** 2026-09-07  
**Gate:** G00  
**Run:** `20260907-203102-950828a1`  
**Estado:** `LANDED`  
**Certificado:** sí  
**Fallos:** 0

## 1. Resultado

Stage 64 dejó de ser sólo un cierre técnico en un prefijo aislado. G00 reconstruyó el árbol,
instaló el producto en `C:\native-sharp`, verificó desde el `nsc.exe` recién copiado el contrato,
el layout y los bytes del paquete, y después completó las dos rutas de enlace sin fallos.

La instalación oficial contiene ahora tres barreras acumulativas:

1. Stage 62 comprueba que las piezas declaren un contrato de toolchain compatible;
2. Stage 63 comprueba que exista la superficie mínima instalada;
3. Stage 64 comprueba que los 76 archivos seleccionados tengan la ruta, el tamaño y el SHA-256
   esperados.

Esta promoción también certifica la corrección de propiedad del prefijo compartido. La presencia
legítima de `bin/nslink.exe` y `lib/NativeAsmCore.lib` ya no se confunde con residuo de NativeSharp;
al mismo tiempo, un archivo inesperado dentro de un subárbol Core administrado continúa siendo un
error fatal.

## 2. Evidencia exacta

| Dato | Valor |
|---|---|
| Directorio de corrida | `A:\nsc-devel\_nscbuilder\history\20260907-203102-950828a1` |
| Inicio | `2026-09-07 20:31:02 -06:00` |
| Fin | `2026-09-07 20:41:04 -06:00` |
| Duración | 601.641 s |
| Etapas operativas | 11/11 |
| Etapas totales con certificación | 12/12 |
| CTest lld-link | 302/302 |
| CTest NSLink | 302/302 |
| Casos registrados por G00 | 324/324 |
| Fallos | 0 |
| Archivos fuente medidos | 1029 al inicio y al final |

Las dos matrices CTest terminaron con estos tiempos:

| Ruta de enlace | Resultado | Tiempo real |
|---|---:|---:|
| lld-link | 302/302 | 124.53 s |
| NSLink | 302/302 | 114.20 s |

## 3. Inmovilidad del árbol

```text
S0 e150f46f4e67d3db630ff85ba1d410dd7193fbe5623d054705d09f5e2cfa512a
S1 e150f46f4e67d3db630ff85ba1d410dd7193fbe5623d054705d09f5e2cfa512a
```

El digest y el conteo de 1029 archivos coincidieron antes de construir y después de ejecutar toda
la matriz. La certificación corresponde a una sola fotografía del compilador: no se mezclaron
fuentes anteriores con documentación, fixtures o implementación editadas durante la corrida.

## 4. Identidad construida e instalada

```text
built_nsc     e9cec52543a6cd709f41b085a9a119c46266b40dfa34dab56cedc9a4a18124fd
installed_nsc e9cec52543a6cd709f41b085a9a119c46266b40dfa34dab56cedc9a4a18124fd
```

La igualdad demuestra que la matriz utilizó el mismo ejecutable que G00 acababa de producir e
instalar. No certificó por accidente un `nsc.exe` anterior presente en el equipo.

Identidad visible del producto promovido:

```text
nsc 0.9.0+build.20260908T023103Z.84b08f1eced9c7f3 (NativeSharp 0.1, stage 64)
build 84b08f1eced9c7f3
```

El instante incluido en el build id está en UTC. La corrida se registra en la zona local `-06:00`;
por eso el identificador contiene 2026-09-08 aunque el acta local corresponde al 7 de septiembre.

## 5. Inventario instalado promovido

Los artefactos oficiales quedaron en:

```text
C:\native-sharp\share\nativesharp\toolchain\NativeSharpInstallation.files
C:\native-sharp\share\nativesharp\toolchain\NativeSharpInstallation.manifest
C:\native-sharp\share\nativesharp\toolchain\NativeSharpToolchain.contract
```

El inventario conserva exactamente:

| Artefacto | Formato | Entradas | Bytes | SHA-256 |
|---|---|---:|---:|---|
| `NativeSharpInstallation.files` | `NSFILES 1` | 76 | 3568 | `d09dbd37423bc156d82a4a5037a2a485aefc69fda0f47a75dbe209f3a4c48b17` |
| `NativeSharpInstallation.manifest` | `NSINSTALL 1` | 76 | 8826 | `e601f94a7097aac70ecb0635c3031b1481142d0a9f8dbf60a9a337c577f9e99e` |
| `NativeSharpToolchain.contract` | `NSTOOLCHAIN 1` | — | 328 | `77f7ef3962c60bdf8c993550fac3ee347798b8c230491ddfa657e63f7273c16e` |

El manifiesto no intenta contener su propio hash: eso crearía una definición circular. Sí autentica
la lista `NSFILES 1`; por tanto, cambiar el conjunto esperado también invalida la instalación.

La etapa de instalación dejó constancia explícita de ambos niveles:

```text
NativeSharp installation verified: C:\native-sharp
installation manifest verified:
  C:\native-sharp\share\nativesharp\toolchain\NativeSharpInstallation.manifest
```

## 6. Qué recorrió G00

La corrida ejecutó, en orden:

1. construcción de `nsc`;
2. instalación y autoverificación del producto;
3. construcción de CTest para lld-link y NSLink;
4. sincronización de las bibliotecas distribuidas;
5. CTest, fixtures y bug triage con lld-link;
6. CTest, fixtures y bug triage con NSLink;
7. comprobación final de fuentes, ejecutables y problemas;
8. escritura del baseline G00.

En Stage 64 el segundo paso es una barrera real. El instalador no puede terminar si el ejecutable
instalado detecta una ruta ausente, bytes modificados, tamaño distinto o residuo en un árbol que
pertenece al paquete.

## 7. Dos universos de pruebas

Los contadores 302 y 324 responden preguntas diferentes:

- **302 CTest por linker:** suites unitarias, contractuales e integraciones registradas por CMake.
  Aquí se encuentra `nsc.stage64.installation-manifest`, con sus 55 comprobaciones directas.
- **324 casos del runner:** fixtures y casos de triage ejecutados en los cuatro cuadrantes de linker
  y optimización.

| Linker | Optimización | Casos | Fallos |
|---|---:|---:|---:|
| lld-link | O0 | 81 | 0 |
| lld-link | O2 | 81 | 0 |
| NSLink | O0 | 81 | 0 |
| NSLink | O2 | 81 | 0 |

Un universo protege la implementación del compilador y sus contratos. El otro confirma el
comportamiento observable de los programas que produce. Ambos fueron necesarios para promover la
misma fotografía.

## 8. Qué quedó promovido

El baseline oficial garantiza ahora que:

- la lista esperada nace del payload fuente, no de lo que casualmente exista en el destino;
- cada ruta se normaliza y se ordena antes de escribir `NSFILES 1`;
- el producto instalado genera y verifica `NSINSTALL 1` con la autoridad SHA-256 compartida;
- falta, cambio de tamaño y cambio de contenido producen diagnósticos precisos;
- los árboles administrados se revisan en busca de residuos;
- herramientas y bibliotecas hermanas pueden coexistir en el prefijo común;
- el compilador construido y el instalado son el mismo binario;
- lld-link y NSLink conservan la matriz completa en O0 y O2.

## 9. Incidente que el baseline cierra

La primera verificación contra el prefijo oficial rechazó:

```text
nsc: error: unexpected installed file 'lib/NativeAsmCore.lib'
```

El diagnóstico era técnicamente fiel a la primera regla, pero la regla tenía una frontera de
propiedad incorrecta: suponía que todo `lib/` pertenecía exclusivamente a NativeSharp. La solución
no fue ignorar ese nombre. El verificador pasó a derivar de la lista esperada los subdirectorios
Core que sí administra y limita a ellos la búsqueda de residuos.

G00 instaló sobre el prefijo compartido real y terminó `LANDED`. Esa ejecución es la prueba que
faltaba de que la corrección funciona fuera del prefijo aislado y que no debilitó la autenticación
de los 76 archivos propios.

## 10. Qué no afirma este baseline

La igualdad `built_nsc == installed_nsc` confirma identidad dentro de una corrida, pero todavía no
demuestra que dos construcciones independientes produzcan los mismos artefactos. El build id es
deliberadamente efímero y habrá que separar esa diferencia esperada de cualquier variación
accidental en Core, contratos, metadata y bibliotecas.

Tampoco formaliza aún el reemplazo o retiro de una instalación anterior. Detectar residuos es una
barrera; definir qué archivos se sustituyen o retiran durante una actualización es el siguiente
contrato operativo.

## 11. Estado de cierre y siguiente paso

Stage 64 está construido, instalado, `LANDED` y certificado. El baseline oficial de E9 deja de ser
Stage 63.

El siguiente bloque E9 propuesto para Stage 65 es la comparación de dos construcciones
independientes. Debe clasificar los artefactos en deterministas y efímeros, normalizar únicamente
la identidad permitida, comparar por categoría y fallar ante cualquier divergencia sin explicación.
Después de esa evidencia se puede cerrar el flujo de actualización y retiro sin residuos.

## 12. Continuación después de este baseline

Stage 66 ya implementó esa comparación y cerró en una sola megaentrega el resto de E9 y E10. Dos
builds limpios coinciden, el PE dejó de incorporar un timestamp accidental, la actualización se
liga a los manifiestos anterior y siguiente y `NSCOMPAT 1` publica la política de versiones. El
capítulo didáctico completo es 150.

Este agregado no promueve retroactivamente Stage 66. El presente documento sigue siendo el acta
inmutable de lo que G00 certificó en Stage 64; Stage 66 necesita su propia corrida y su propia
identidad instalada.
