# 08 — Inventario del archivo histórico — Revisión 2

## 1. Fuente y alcance

Este documento registra la primera observación directa de `A:\native-sharp-historicos` usada para
ampliar el plan de la obra. La carpeta se trató como sólo lectura. No se movió, renombró,
descomprimió ni corrigió ningún artefacto.

La medición corresponde al 8 de septiembre de 2026. Describe existencia física; todavía no
certifica todos los ZIP ni todas sus extracciones.

## 2. Magnitud

| Medida | Resultado |
|---|---:|
| directorios de nivel raíz | 126 |
| archivos recursivos | 89,580 |
| tamaño total | 2,370,237,534 bytes |
| `.md` | 9,744 |
| `.zip` | 401 |
| `.patch` | 1,080 |
| `MANIFEST_SHA256.txt` | 84 |
| `.log` | 14,835 |
| `.cpp` | 15,859 |
| `.h` | 16,855 |
| `.ns` | 8,234 |
| `.obj` | 825 |
| `.lib` | 282 |
| `.dll` | 218 |
| `.exe` | 214 |

La diferencia entre 401 ZIP y 126 directorios de primer nivel demuestra que no todos los paquetes
están extraídos en un directorio homónimo. También existen ZIP dentro de árboles acumulativos.

## 3. Por qué los números brutos engañan

El archivo mezcla:

1. paquetes originales descargados desde conversaciones;
2. extracciones de esos paquetes;
3. snapshots acumulativos que repiten todo lo anterior;
4. incrementales SOURCE_ONLY;
5. candidates previos al cierre;
6. handovers con código o ZIP anidado;
7. copias `(1)`, `(2)` o `_1` descargadas varias veces;
8. directorios de build incluidos accidentalmente;
9. binarios y logs de verificación;
10. proyectos relacionados: NSASM, NSLink, nsbind, bindings, Saru y OpenGL.

Por tanto:

```text
9,744 Markdown físicos
≠ 9,744 documentos históricos independientes

15,859 .cpp físicos
≠ 15,859 archivos fuente creados por NativeSharp
```

El catálogo lógico deberá deduplicar por contenido, procedencia y rol.

## 4. Documento histórico preexistente

El archivo contiene:

```text
NativeSharp_119_Archivo_Historico_Completo_Como_Se_Construyo.md
```

Se identifica a sí mismo como “revisión 2” y reconstruye 259 entradas de nivel raíz, unos 669 MB,
entre el 16 y el 31 de agosto de 2026. Declara haber leído documentos sueltos, pero no haber abierto
los ZIP uno por uno.

Para esta obra será una fuente secundaria de gran valor, no la última autoridad. Rev 2 añade:

- medición recursiva del archivo completo;
- inspección de árboles extraídos;
- conteos de archivos/docs/manifests por snapshot;
- comprobación futura ZIP↔extracción;
- hashes para distinguir copias de revisiones;
- lectura focal de worklogs y evidencia.

## 5. Documentos normativos tempranos

La subcarpeta `md\` conserva:

```text
00_introduccion.md
01_nativesharp_language_specification_0_1(1).md
02_nativesharp_lexical_structure_and_grammar.md
03_nativesharp_type_system_and_semantic_rules.md
```

El snapshot `NativeSharp_Parte1_snapshot_00_03` contiene los mismos roles con nombres canónicos,
además de:

```text
MANIFEST.sha256
nativesharp_curso_1_compilador_procedural_revisado.md
README.md
WORKLOG_CURRENT_STATE.md
```

Su worklog declara correctamente que todavía no existe compilador. Esto convierte el snapshot en
una frontera histórica de enorme importancia: primero hubo contrato, después implementación.

## 6. Primer árbol C++ observado

`NativeSharp_snapshot_00_06` es el primer árbol físico observado. Contiene:

```text
CMakeLists.txt
src/nsc/main.cpp
src/Driver/Driver.cpp
src/Target/TargetInfo.cpp
src/Target/TargetDataLayout.cpp
headers correspondientes
tests/unit/TargetDataLayoutTests.cpp
documentos 00–06
evidencia GCC/Clang
referencia diferencial ABI Windows x64
```

El worklog registra 4/4 CTest con GCC 14.2 y 4/4 con Clang 17, warnings-as-errors. El driver ya
implementa `--help`, `--version`, `--print-target` y `--print-data-layout`; compilar `.ns` todavía
devuelve un error deliberado.

La solicitud de reconstruir “desde un `main` vacío” exige un checkpoint anterior a éste, pero ese
checkpoint será `RECONSTRUIDO_PEDAGOGICO`, no presentado como snapshot original.

## 7. Entrega conjunta 07–10

`NativeSharp_delivery_07_10` tiene 116 archivos, ocho Markdown y un manifest. Contiene el frontend
completo de SourceManager/Diagnostics, Lexer, Parser/AST y Sema, junto con suites unitarias,
fixtures positivos/negativos y evidencia GCC/Clang/sanitizers.

Históricamente los cuatro stages llegaron juntos al archivo. Pedagógicamente la obra los dividirá
en checkpoints pequeños, pero cada uno deberá marcarse como una extracción didáctica desde una
entrega conjunta, no como ZIP histórico independiente.

## 8. Entregas 11–20

Existen directorios extraídos `NativeSharp_delivery_11` a `NativeSharp_delivery_19`, cada uno con
manifest y documentación normativa/paso-a-paso. La progresión física observada es:

| Entrega | Archivos | Markdown | Hito |
|---:|---:|---:|---|
| 11 | 123 | 6 | HIR |
| 12 | 158 | 6 | MIR no-SSA |
| 13 | 183 | 6 | CFG |
| 14 | 204 | 6 | Dominance |
| 15 | 238 | 6 | SSA |
| 16 | 288 | 6 | Pass Manager/optimizaciones |
| 17 | 290 | 6 | LLVM lowering |
| 18 | 326 | 10 | TargetMachine/COFF |
| 19 | 379 | 14 | Runtime/linking/PE |

No se observó directorio ni ZIP de primer nivel `NativeSharp_delivery_20`. Stage 20 está preservado
en `NativeSharp_Parte1_Proyecto_Completo`, con sus documentos, matriz y worklog. Sin embargo, la
conversación `05 Continuar NativeSharp procedural` dice que un paquete posterior se preparó desde
un `NativeSharp_delivery_20.zip` real y registra 83/83 pruebas. Esto abre una búsqueda específica:
el artefacto pudo renombrarse, quedar anidado o no sobrevivir como entrada superior. El libro
`NativeSharp_Libro_Universitario_Capitulos_00_20` conserva 21 capítulos y dos volúmenes continuos.

## 9. Entregas 21–30

Cada entrega tiene ZIP y árbol extraído acumulativo. El número de archivos crece de 461 en Stage 21
a 715 en Stage 30. Los snapshots conservan documentación heredada, por lo que el Markdown nuevo de
cada ronda debe localizarse por nombre y diff, no por conteo bruto.

Dos límites históricos deben preservarse:

- Stage 21 y 22 registran que el full CMake/CTest no se ejecutó por dependencias no disponibles;
- Stage 23 ya registra 87/87 con GCC, Clang y rebuild del árbol exacto.

No se retroescribirá “PASS completo” en 21/22 usando resultados posteriores. La evidencia posterior
demuestra compatibilidad acumulativa, no la ejecución contemporánea que faltó.

## 10. Macro A, Stage 37 y recoveries 35/36

Los directorios observados fijan la genealogía material:

```text
NativeSharp_Parte3_MacroA_Stages31_34
  → NativeSharp_Parte3_Stage37_Static_Initialization
  → NativeSharp_Parte3_Stage35_Recovery_on_Stage37
  → NativeSharp_Parte3_Stage36_Recovery_on_Stage37plus35
  → NativeSharp_Parte3_Stage38_Properties_Indexers_Operators
```

Los worklogs `WORKLOG_CURRENT_STATE_35_RECOVERY.md` y `_36_RECOVERY.md` existen físicamente. La
obra deberá narrar stages lógicos 35/36 y orden material de recuperación como dos ejes diferentes.

## 11. Stages 38–44

Hay árboles extraídos, manifests y worklogs acumulativos. Se observaron identidades correctas en
los worklogs, aunque algunas descripciones de `CMakeLists.txt` quedaron obsoletas: los árboles 42 y
43 todavía pueden describirse como Stage 41. Es una deuda de identidad histórica y una lección de
por qué `DESCRIPTION` no basta como autoridad.

## 12. Stage 45

El archivo conserva árboles/ZIP para:

```text
45.1  Syntax/AST
45.2  Exception Sema
45.3  Cleanup scopes
45.4  Finally flow
45.5  Using/IDisposable
45.6  HIR exceptional CFG
45.7  MIR exceptional CFG
45.9  LLVM Win64 EH básico
45.10 LLVM cleanup funclets
45.11 Typed catch/RTTI
45.12 Linker/PE/EH
Final_Certified
```

No se observó paquete 45.8 con ese nombre. El runtime de excepciones puede estar absorbido en otro
delta; esto se investigará mediante comparación 45.7→45.9.

La conversación `Implementar Stage 45` añade otra frontera: 45.13–45.19 fueron checkpoints internos
de una sola macroentrega de validación, cerrada lógicamente como `0.2.0-dev45.19`. No se esperan
necesariamente siete árboles fuente separados. El SHA-256 calculado de
`NativeSharp_Parte3_Stage45_Final_Certified.zip` coincide exactamente con el hash citado en el chat:
`a106c777c76652969ceaaf236f6335e9b5db7f71d9388e9a585ddcbcbf09bef5`.

## 13. Stages 46–50

Los paquetes 46–48 están principalmente como ZIP de nivel raíz; incluyen candidatos, FULL,
SOURCE_ONLY y copias. Stage 49 tiene árbol FULL extraído. Stage 50 conserva once checkpoints FULL,
sus SOURCE_ONLY y el FULL final.

Anomalías observadas/documentadas:

- `Stage48_1_CANDIDATE` precede al FULL;
- varias copias `(1)` tienen mismo tamaño y deben compararse por hash;
- el `CMakeLists.txt` de Stage49 conserva una descripción vieja de Stage48.3;
- Stage50 vuelve a `VERSION 0.1.0` en CMake aunque la línea anterior usaba 0.2.0; los chats muestran
  que `0.1.0-dev50` se usó repetidamente durante certificaciones, por lo que puede ser reinicio o
  corrección deliberada de un reloj de producto y no un simple typo;
- `Stage50_5_CHECKPOINT_FULL` incluye builds y crece anormalmente;
- la extracción `Stage50_11_CHECKPOINT_SOURCE_ONLY` está vacía.

Ninguna anomalía debe corregirse en el archivo. Debe explicarse y usarse para diseñar mejores gates.

## 14. Stages 51–56 y maduración

El archivo conserva múltiples intentos de Stage 51, críticas y cierres de Stage 52, paquetes 53–55,
snapshots de Stage 56 y compiladores completos sucesivos. Aquí nace la convención de dos ZIP:

```text
incremental de la ronda
+
compilador completo acumulativo
```

El motivo fue un fallo real de CMake en Windows: un CMakeLists acumulativo referenciaba tests de
rondas que no habían llegado al checkout local. La convención comienza desde Stage 53.4 y no debe
proyectarse retrospectivamente sobre entregas anteriores.

## 15. Proyectos hermanos

NSASM, NSLink, nsbind, bindings, Saru y tutoriales son evidencia del ecosistema y de presión real
sobre el compilador, pero no todos pertenecen al cuerpo principal de “cómo se construyó `nsc`”. Se
clasificarán así:

- `CAUSA_CONSUMIDORA`: descubrió un bug o necesidad del compilador;
- `GATE_EXTERNO`: verifica ABI/linking/producto;
- `ECOSISTEMA`: importante para contexto, no delta de `nsc`;
- `OBRA_SEPARADA`: merece documentación propia.

## 16. Resultado para el índice

La Rev 2 conserva los capítulos 000–210, pero añade interludios documentales para:

- contrato antes de código;
- entrega conjunta 07–10;
- ausencia de Delivery 20 separado;
- no-ejecución contemporánea en 21/22;
- recuperación fuera de orden 35/36;
- Stage 45.8 absorbido;
- candidates, FULL y SOURCE_ONLY;
- regresiones de identidad/versionado;
- nacimiento de la doble entrega;
- primera ejecución real y bugs encontrados por consumidores.
