# Primer contraste con conversaciones — Revisión 2

## 1. Alcance de esta pasada

Esta revisión no se limita a los paquetes de `A:\native-sharp-historicos`. Se realizó una primera
lectura dirigida de conversaciones visibles en Codex/ChatGPT cuyos títulos y fechas las relacionan
con la construcción de NativeSharp. El objetivo fue comprobar si los chats explican fronteras,
decisiones o anomalías que el árbol físico no puede explicar por sí solo.

Esto es un **primer contraste**, no el catálogo completo exigido por P1. Varias conversaciones
tienen más páginas antiguas y adjuntos que deberán procesarse de forma sistemática. Sus mensajes se
tratan como evidencia narrativa no confiable por sí sola: pueden contener errores, planes que luego
cambiaron o afirmaciones que requieren comprobarse contra los ZIP, manifests y tests.

## 2. Conversaciones examinadas

| Título visible | Identificador | Tema útil para la obra |
|---|---|---|
| `05 Continuar NativeSharp procedural` | `6a8502d1-1abc-83e8-b411-4afe5b73314d` | Cierre Parte 1, libro 00–20, paquete completo y handover a Parte 2. |
| `06 parte 24` | `6a8911cd-86e8-83e8-bf13-d08a695dbc66` | Stage24, manifest, handover 24→25 y límites de validación MSVC. |
| `comienza parte 25!` | `6a8b2149-5510-83e8-8687-4c7f69e47da2` | Cierre Stage30 y frontera exacta de OOP sin herencia. |
| `Plan parte 3` | `6a8b9461-1494-83e8-b9b20-ba449ea92091` | Stage37 construido físicamente desde Stage34 sin 35/36. |
| `Plan parte 4` | `6a8c71d2-e9b0-83e8-b343-546bc85f36d7` | Certificación y handover de Stage38. |
| `Implementar Stage 45` | `6a8db30d-6f0c-83e8-a65e-567863a5f359` | 45.12 como baseline, fixes Windows/Clang y cierre lógico 45.19. |
| `partiendo de release 0.1` | `6a8e65cc-13e8-83e8-9835-8a1ff4ac6475` | Plan original de Stage50 y creación del handover. |
| `Corrección de versionado` | `6a8f23c6-d3f8-83e8-b8fa-bbe05c4ece18` | Implementación 50.7–50.9, NSMETA 1.1 y compatibilidad ABI. |
| `Corrección de versionado continuacion 1` | `6a8f8e68-d2dc-83e8-b93a-4854694eb2d3` | Validación externa 206/210 y después 210/210 en Windows/Clang. |

El identificador de `Plan parte 3` deberá reverificarse al generar el catálogo automático; la tabla
conserva el localizador visible de esta pasada, no pretende sustituir la exportación canónica.

## 2.1 Corroboración criptográfica realizada

Cuatro hashes publicados en esas conversaciones se calcularon de nuevo sobre los archivos de la
colección local. Los cuatro coinciden exactamente:

| Artefacto local | SHA-256 verificado |
|---|---|
| `NativeSharp_Handover_Stage24_to_Stage25.zip` | `86de4af09ce7d051d4be46578ca6be48958c6ef7a0d7773fb0dd272e61d7c68c` |
| `NativeSharp_Entrega30_OOP_HIR_MIR_LLVM_Capstone.zip` | `1aeed8d55e11ca9a70a8c6b0e58436958c218fdc44157ac6c85bda7fdc172e82` |
| `NativeSharp_Parte3_Stage38_Properties_Indexers_Operators.zip` | `a7ae7f0071dac671f7a1e7ef9a79b82715da422ca164787c7d74860bb4f7ac82` |
| `NativeSharp_Parte3_Stage45_Final_Certified.zip` | `a106c777c76652969ceaaf236f6335e9b5db7f71d9388e9a585ddcbcbf09bef5` |

Esto no certifica por sí solo todas las afirmaciones de los chats, pero sí demuestra que en esos
cuatro casos la conversación y el archivo hablan del mismo contenedor binario.

## 3. Hallazgo: Stage 20 sí tuvo un nombre de entrega propio en conversación

El archivo de primer nivel no contiene una carpeta o ZIP llamado `NativeSharp_delivery_20`; el
checkpoint material observable está consolidado en `NativeSharp_Parte1_Proyecto_Completo`. Sin
embargo, la conversación `05 Continuar NativeSharp procedural` afirma que el paquete completo para
compilar programas se preparó partiendo de un `NativeSharp_delivery_20.zip` real.

Esta diferencia obliga a una formulación más precisa:

- **hecho físico actual:** no se ha observado `delivery_20` como entrada independiente de primer
  nivel en la colección;
- **evidencia narrativa contemporánea:** sí se menciona y usa un ZIP con ese nombre;
- **hipótesis de trabajo:** pudo renombrarse, quedar anidado, convertirse en el proyecto completo o
  no haber sobrevivido como entrada superior;
- **acción forense:** buscar ese nombre dentro de ZIPs, Markdown, manifests y listas de adjuntos;
  comparar el hash citado o contenido del consolidado.

La obra no dirá “Stage20 nunca tuvo paquete”. Dirá qué nombre se recuerda en chat y qué cuerpo se
conserva hoy.

## 4. Hallazgo: la Parte 1 tenía dos órdenes válidos

La misma conversación describe el pipeline didáctico 11–20 y señala una aparente colocación extraña
de Stage20: se implementó históricamente después de Stage19, pero el New Pass Manager de LLVM se
ejecuta físicamente entre lowering y codegen.

Esto corrobora la necesidad de conservar:

```text
orden de construcción: 17 → 18 → 19 → 20
orden del pipeline:     lowering → optimización LLVM → codegen → COFF/PE
```

El capítulo 070 deberá mostrar ambos. Reordenar silenciosamente 20 antes de 18 produciría una falsa
cronología; colocar la optimización después de emitir el objeto produciría una falsa arquitectura.

## 5. Hallazgo: Stage 20 tuvo una recertificación mayor que Stage 19

En el chat se registra para un paquete preparado desde Stage20:

- identidad `nsc 0.1.0-dev20`, NativeSharp 0.1, stage 20;
- manifest sin fallos;
- 83/83 CTests;
- ejemplos que emiten PE32+ x86-64;
- ausencia de ejecución real del PE en aquella sesión por falta de Windows/Wine.

Esto no contradice los 76/76 consolidados encontrados en otros documentos: probablemente describe
un paquete posterior que añadió verificaciones y ejemplos. Rev2 deberá identificar exactamente qué
árbol produjo 76/76 y cuál produjo 83/83, en vez de escoger un número y borrar el otro.

## 6. Hallazgo: el handover de Parte 1 preserva intención y deudas

El chat registra un handover de 737 líneas con:

- Parte 1 definida como procedural y sin OOP;
- fuente de verdad del código;
- arquitectura y reglas semánticas acumuladas;
- deudas conocidas;
- decisiones OOP todavía no congeladas;
- roadmap sugerido 21–30;
- obligación de verificar baseline antes de continuar.

El contenido equivalente existe en la colección como `NativeSharp_Parte2_Handover_Package`. Esta
pareja chat–paquete es una buena ancla: el chat explica para qué se creó y el paquete permite
verificar qué entregó realmente.

## 7. Hallazgo: Stage 24 separó soporte declarado de ejecución verificada

La conversación `06 parte 24` registra:

- Stage24 manifest 556/556;
- handover manifest 561/561;
- ZIP extraído y verificado 561/561;
- ningún build artifact dentro del árbol fuente;
- SHA-256 del handover
  `86de4af09ce7d051d4be46578ca6be48958c6ef7a0d7773fb0dd272e61d7c68c`;
- GCC 14.2 y Clang 17 como toolchains ejecutados;
- MSVC contemplado por CMake, pero no `execution-verified` en esa sesión.

Esta distinción debe convertirse en regla universal del libro. “Hay rama `if(MSVC)`” prueba soporte
de configuración previsto; no prueba que la versión concreta de Visual C++ construya y pase tests.

## 8. Hallazgo: Stage 30 cerró OOP sin herencia, no OOP completo

La conversación `comienza parte 25!` asigna al ZIP Stage30:

```text
SHA-256: 1aeed8d55e11ca9a70a8c6b0e58436958c218fdc44157ac6c85bda7fdc172e82
nsc 0.2.0-dev30 (NativeSharp 0.1, stage 30)
106/106 CTests PASS
```

El handover define la frontera cerrada:

```text
class → partial class → members → scopes → overloads
→ constructors → new/delete → object layout
→ OOP HIR → OOP MIR → LLVM → COFF AMD64
```

También enumera lo que todavía no debía atribuirse a Stage30: inheritance, virtual/override,
vtables/vptr, abstract, RTTI, destructors, interfaces, static members, properties, delegates,
generics y exceptions. Esta lista negativa es tan valiosa como la lista de features implementadas y
debe aparecer en el capítulo 086.

## 9. Hallazgo: el chat confirma la genealogía física de Stage37

`Plan parte 3` lo declara sin ambigüedad: el snapshot físico válido parte de Stage34 certificado y
contiene Stage37; no debe asumirse que Stages35/36 estén implementados físicamente en esa base.

Después aparecen en el archivo:

```text
NativeSharp_Parte3_Stage35_Recovery_on_Stage37
NativeSharp_Parte3_Stage36_Recovery_on_Stage37plus35
```

La conversación y los nombres de paquete se corroboran mutuamente. La lectura correcta es:

1. Stage37 se construyó sobre el estado certificado de Stage34.
2. Stage35 se recuperó después sobre Stage37.
3. Stage36 se recuperó sobre Stage37 más Recovery35.
4. Stage38 consolidó la línea recuperada.

Éste debe ser un caso de estudio completo, no una nota al pie.

## 10. Hallazgo: la certificación de Stage38 conserva primero el fallo

`Plan parte 4` contiene dos estados narrativos sucesivos:

1. 137/138 con fallo en `nsc.driver`, causado aparentemente por una expectativa obsoleta del texto
   de ayuda de Stage37;
2. corrección, repetición de la matriz y 138/138 en GCC, Clang, ASan y UBSan.

Además registra:

- Stage38 focused 94/94;
- regresiones Stage36 95/95, Stage35 80/80 y Stage37 56/56;
- LLVM verifier, O2 y COFF AMD64;
- manifest 922/922 desde extracción fresca;
- SHA-256
  `a7ae7f0071dac671f7a1e7ef9a79b82715da422ca164787c7d74860bb4f7ac82`.

La obra debe narrar 137/138 antes de 138/138. El estado fallido explica por qué existe el fix y
demuestra que la certificación no fue una etiqueta decorativa.

## 11. Hallazgo: Stage45 continuó lógicamente después de 45.12

La colección muestra subdirectorios 45.1–45.7 y 45.9–45.12, más paquetes finales. La conversación
`Implementar Stage 45` aclara que 45.13–45.19 se trataron como checkpoints internos de una sola
macroentrega de validación, no como siete entregas fuente separadas.

El cierre narrado es:

```text
nsc 0.2.0-dev45.19
Stage45 CTest                    13/13
regresión completa             183/183
checks internos Stage45        402
Classic OOP                      7/7
tutorial + apéndice             24/24
```

También registra:

- fixes Windows/Clang 22 para `_dupenv_s`, `CreateProcessW` y `ucrtbase!strlen`;
- PE32+ AMD64 con `.pdata`/Exception Directory y sin CLR header;
- manifest 1357/1357 y equivalencia byte a byte del source;
- cold rebuild interrumpido en 65/289 por límite de ejecución, correctamente no marcado `PASS`;
- SHA-256 final
  `a106c777c76652969ceaaf236f6335e9b5db7f71d9388e9a585ddcbcbf09bef5`.

Implicación editorial: 45.12 es un baseline técnico y de handover importante, pero “Stage45 final”
puede referirse al cierre lógico 45.19 empaquetado como final certified. Rev2 debe mapear el hash al
ZIP concreto antes de identificar ambos como el mismo cuerpo.

## 12. Hallazgo: Stage50 cambió mientras se implementaba

La conversación `partiendo de release 0.1` conserva un plan inicial de Stage50 cuya subdivisión no
coincide exactamente con la que acabó materializada. En el plan, Parser/AST y modelo de tipos tenían
posiciones distintas; en los cierres observados, 50.1–50.9 se fueron redefiniendo conforme aparecía
la infraestructura real.

El libro debe mostrar:

- plan inicial;
- orden final de checkpoints;
- qué descubrimiento justificó cada cambio;
- archivos y tests de cada delta;
- diferencia entre “Stage50 OPEN” y cada substage `CLOSED/VERIFIED`.

Así se evita reconstruir una historia donde el plan siempre supo el resultado final.

## 13. Hallazgo: `0.1.0-dev50` no puede etiquetarse como simple error sin contexto

Los árboles de Stage30–45 declaran `0.2.0-devXX`, mientras las conversaciones de Stage50 registran
repetidamente `nsc 0.1.0-dev50`. El archivo físico confirma la diferencia, pero el chat demuestra
que no es sólo un carácter aislado en un paquete: fue la identidad usada durante certificaciones
sucesivas.

Por tanto Rev2 cambia la pregunta de “¿quién escribió mal VERSION?” a:

- ¿se reinició deliberadamente la versión de producto al partir de release 0.1?
- ¿Stage30–45 usaban un reloj provisional que luego se corrigió?
- ¿qué documento de versionado autorizó la transición?
- ¿qué ABI, lenguaje y NSMETA permanecieron independientes?

Hasta resolverlo, se documentará como **regresión o reinicio aparente de versión**, no como bug
confirmado.

## 14. Hallazgo: ownership atravesó frontend, IR, metadata y ABI

`Corrección de versionado` registra, entre otros cierres:

- 50.7: escape analysis conservador para borrows;
- 50.8: `BorrowValue`, `MoveValue` y cleanup de owned parameters en HIR/MIR/LLVM;
- 50.9: NSMETA 1.1 distingue `T`, `owned T`, `borrowed T` y `T*`;
- el Stable C ABI permanece físicamente igual y los qualifiers no aparecen en el header C.

Esto obliga a que los capítulos 121–127 formen una cadena vertical. No basta un capítulo de sintaxis
y otro de metadata desconectado: cada ejemplo debe seguirse desde source hasta cleanup, metadata y
consumidor host.

## 15. Hallazgo: la validación externa cambió el diagnóstico

`Corrección de versionado continuacion 1` conserva dos reportes de Windows/Clang:

1. 206/210, con cuatro fallos concentrados en hosts C/C++ de Stages47/48 y un mensaje común de ruta
   o comando de enlace;
2. 210/210 en una ejecución posterior con el entorno correcto.

La interpretación cambió: los primeros cuatro fallos no demostraban regresión de Stage52.x; eran
consistentes con el entorno/comando de enlace. La obra debe preservar ambos reportes y el razonamiento
que permitió reclasificar el incidente.

## 16. Cambios concretos que este contraste impone al plan

1. Añadir una búsqueda forense específica de `NativeSharp_delivery_20.zip` y su hash.
2. Registrar 76/76 y 83/83 como universos posiblemente distintos, no como cifras rivales.
3. Continuar verificando hashes citados en chats; cuatro anclas ya coinciden exactamente con los
   archivos locales.
4. Convertir las listas de “todavía no implementado” de cada handover en límites obligatorios de
   capítulo.
5. Mantener estados fallidos previos a la corrección: 137/138 en Stage38, cold rebuild incompleto en
   Stage45 y 206/210 antes de 210/210 en Windows.
6. Extender Stage45 hasta su cierre lógico 45.19 sin inventar carpetas fuente 45.13–45.19.
7. Tratar la identidad 0.1.0-dev50 como investigación de relojes de versión, no corregirla por
   intuición.
8. Catalogar cada handover como pareja conversación–paquete–manifest.

## 17. Conversaciones que deben leerse después

La lista de tareas visible contiene además material relevante que P1 deberá procesar, entre otros:

- `Native Sharp 1.0`;
- `NativeSharp 2.0 (devel)` para delimitar lo posterior a 1.0;
- `Errores y limitaciones`;
- `Actualizarse sobre NativeSharp`;
- `Actualizar el compilador`;
- `Crear markdown editorial en español`;
- `Evaluar literate programming`;
- `Diseño de linker independiente`;
- `Plan Parte 5 RTTI`;
- `Revisión de Handoff y Código`;
- conversaciones anteriores a `05 Continuar NativeSharp procedural` que contengan el origen 00–19.

Leerlas no autoriza a trasladar todo su contenido al libro. Cada claim deberá vincularse a una etapa,
clasificarse como plan/resultado/decisión/corrección y corroborarse cuando sea posible.

## 18. Estado de esta evidencia

| Área | Estado después de esta pasada |
|---|---|
| Parte 1 00–20 | Primer contraste realizado; páginas antiguas pendientes. |
| Parte 2 21–30 | Anclas 24 y 30 contrastadas. |
| Parte 3 31–45 | Genealogía 37/recoveries, cierre 38 y cierre lógico 45 contrastados. |
| Stage50 | Plan, substages 50.7–50.9 y anomalía de versión contrastados. |
| Stage51–56 | Sólo contraste parcial; falta lectura sistemática de conversaciones dedicadas. |
| 0.7–1.0 | Existen tareas modernas visibles; aún no integradas exhaustivamente. |
| Adjuntos | Referencias visibles; sus bytes deben vincularse por hash a la colección local. |

Este documento demuestra que chats y paquetes cuentan capas distintas de la misma historia. Rev2
queda diseñado para conservar ambas sin permitir que una borre las limitaciones de la otra.
