# Protocolo forense de ZIP, manifests y duplicados — Revisión 2

## 1. Objetivo

La colección histórica es demasiado grande y heterogénea para interpretarla por nombres de archivo. Contiene árboles extraídos, ZIPs, copias con sufijos, entregas completas, incrementales, candidatos, paquetes source-only, builds accidentales, documentos, binarios y productos vecinos. Este protocolo establece cómo convertir esa colección en evidencia auditable sin modificarla.

No busca “limpiar” el archivo histórico. Busca describirlo tal como existe y producir, dentro de Rev2, un catálogo derivado que cualquier persona pueda volver a generar.

## 2. Invariantes de seguridad

1. `A:\native-sharp-historicos` se monta y trata como sólo lectura.
2. No se renombra, mueve, borra, extrae encima ni corrige ningún original.
3. Toda extracción temporal ocurre fuera de la raíz histórica.
4. Todo resultado permanente se escribe bajo `implementation-step-by-step-rev2\evidence` o una revisión posterior explícita.
5. No se ejecuta un binario histórico desconocido para catalogarlo.
6. Antes de compilar un checkpoint se crea un entorno aislado y se inspeccionan scripts, CMake y dependencias.
7. Un paquete sospechoso, corrupto o vacío sigue siendo evidencia; se marca, no se repara silenciosamente.
8. El original y el artefacto derivado siempre se relacionan mediante hash y ruta.

## 3. Productos derivados previstos

```text
implementation-step-by-step-rev2/
└── evidence/
    ├── catalog/
    │   ├── inventory.tsv
    │   ├── inventory.jsonl
    │   ├── zip-members.jsonl
    │   ├── manifests.jsonl
    │   └── parse-errors.jsonl
    ├── hashes/
    │   ├── files-sha256.tsv
    │   ├── duplicate-files.tsv
    │   ├── normalized-trees.tsv
    │   └── duplicate-packages.tsv
    ├── genealogy/
    │   ├── nodes.jsonl
    │   ├── edges.jsonl
    │   ├── deltas.tsv
    │   └── unresolved-edges.md
    ├── certification/
    │   ├── historical-results.tsv
    │   ├── reproduced-results.tsv
    │   └── environments/
    └── reports/
        ├── anomalies.md
        ├── package-families.md
        └── coverage-by-chapter.md
```

La creación de estos archivos es una fase posterior a la planificación. Este protocolo define su contrato para evitar que distintas rondas produzcan catálogos incompatibles.

## 4. Fase A — Inventario físico sin abrir paquetes

### 4.1 Un registro por entrada

Para cada archivo y directorio se capturan al menos:

- ruta relativa a la raíz histórica;
- tipo: archivo, directorio, enlace u otro;
- tamaño en bytes;
- extensión normalizada sin perder el nombre original;
- timestamps disponibles y su semántica conocida;
- atributos del sistema de archivos;
- profundidad;
- nombre de la entrada superior a la que pertenece;
- SHA-256 para archivos regulares;
- error de lectura, si existe.

### 4.2 Los timestamps no son una cronología suficiente

Una extracción puede asignar una misma fecha a miles de archivos o preservar fechas internas que preceden a la copia. Un ZIP puede haber sido descargado después de que se creara su contenido. Por eso se guardarán por separado:

- timestamp del contenedor en el sistema de archivos;
- timestamp declarado por cada miembro del ZIP;
- fechas escritas en documentos/worklogs;
- orden inferido por contenido;
- fecha de la sesión de catalogación.

Nunca se traducirá automáticamente una diferencia de timestamps en “tiempo de desarrollo”.

### 4.3 Conteos base de esta revisión

El inventario inicial observado aporta una comprobación de humo para futuras ejecuciones:

| Medida | Valor observado |
|---|---:|
| Entradas superiores | 259 aproximadamente, según el documento histórico previo; el inventario actual debe calcularlo de nuevo con reglas explícitas |
| Directorios superiores | 126 |
| Archivos totales | 89,580 |
| Bytes totales | 2,370,237,534 |
| Markdown | 9,744 |
| ZIP | 401 |
| Patches | 1,080 |
| `MANIFEST_SHA256.txt` | 84 |
| C++ `.cpp` | 15,859 |
| Headers `.h` | 16,855 |
| NativeSharp `.ns` | 8,234 |
| Logs `.log` | 14,835 |

Una diferencia futura no implica corrupción por sí sola: primero se compararán alcance, enlaces, archivos ocultos, reglas de exclusión y cambios intencionales de la colección.

## 5. Fase B — Inspección segura de contenedores

### 5.1 Enumerar antes de extraer

Para cada ZIP se registra:

- hash y tamaño del contenedor;
- método de compresión;
- cantidad de miembros;
- tamaños comprimido y expandido;
- rutas internas originales;
- CRC si está disponible;
- miembros duplicados por nombre;
- rutas absolutas, `..`, prefijos de unidad o nombres reservados;
- cifrado, errores o formato no soportado.

La enumeración sirve para detectar path traversal, paquetes anidados y bombas de expansión antes de escribir un solo miembro.

### 5.2 Extracción aislada

Cada contenedor se extrae en un directorio temporal único. Antes se verifica que toda ruta normalizada permanezca dentro de ese directorio. No se permiten:

- rutas absolutas;
- componentes `..` que escapen;
- enlaces que apunten fuera;
- sobrescritura de un miembro por otro tras normalizar mayúsculas/minúsculas;
- escritura sobre una extracción anterior.

Al terminar, la extracción se hashea y cataloga; después puede descartarse porque el catálogo conserva la relación reproducible con el ZIP.

### 5.3 Paquetes anidados

Un ZIP dentro de otro no se confunde con un archivo fuente. Se registra una arista `contains-package`, se calcula su hash como miembro y sólo se abre en una segunda fase controlada. Así se evita contar el mismo proyecto repetidamente como si fueran versiones distintas.

## 6. Fase C — Normalización de árboles

### 6.1 El problema del directorio envolvente

Dos ZIPs pueden contener el mismo árbol con prefijos diferentes:

```text
NativeSharp_Stage47_FULL/native-sharp/...
native-sharp/...
./native-sharp/...
```

El protocolo calcula al menos dos identidades:

1. **Identidad cruda:** conserva exactamente todas las rutas del paquete.
2. **Identidad normalizada:** elimina sólo envolventes triviales previamente definidas y normaliza separadores, sin alterar nombres internos.

Si dos paquetes sólo coinciden bajo una normalización agresiva, no se llaman duplicados exactos; se llaman equivalentes bajo esa regla y se registra la regla.

### 6.2 Qué no debe normalizarse

No se modifican para calcular identidad histórica:

- contenido o finales de línea;
- mayúsculas/minúsculas de nombres;
- espacios o Unicode;
- timestamps incrustados en archivos;
- binarios no deterministas;
- rutas internas no triviales;
- archivos de build o logs.

Puede existir una identidad adicional “sólo fuentes”, pero nunca reemplaza la identidad cruda.

### 6.3 Hash de árbol

Un hash de árbol reproducible se deriva de una secuencia ordenada que incluya, por archivo:

```text
tipo NUL ruta-normalizada NUL tamaño NUL sha256-del-contenido LF
```

La especificación exacta, encoding y orden deben versionarse. Directorios vacíos se incluyen si el formato pretende distinguirlos. Symlinks, cuando existan, se registran por destino textual y nunca se siguen fuera del árbol.

## 7. Fase D — Lectura y validación de manifests

### 7.1 Detectar dialecto

No se supone que los 84 archivos `MANIFEST_SHA256.txt` tengan exactamente el mismo formato. Para cada uno se detecta:

- orden hash/ruta;
- separador;
- comillas o escaping;
- uso de `*` para modo binario;
- encoding y BOM;
- separador de ruta;
- tratamiento de espacios;
- inclusión o exclusión del manifest;
- raíz asumida.

### 7.2 Estados de una entrada

Cada línea parseable obtiene uno de estos estados:

- `MATCH`: ruta presente y hash correcto;
- `MISSING`: declarada pero ausente;
- `MISMATCH`: presente con otro contenido;
- `EXTRA`: archivo real no declarado, si el manifest afirma cobertura total;
- `AMBIGUOUS_PATH`: la ruta no se resuelve de manera única;
- `UNSUPPORTED`: sintaxis aún no interpretada;
- `SELF_REFERENCE`: entrada del propio manifest, que requiere regla especial.

### 7.3 Cobertura

Un manifest con todas sus líneas en `MATCH` no necesariamente cubre el paquete completo. Se registrará:

- cantidad de archivos declarados;
- cantidad de archivos físicos en alcance;
- extras por categoría;
- patrón de exclusión inferido o documentado;
- porcentaje y límites de cobertura.

## 8. Fase E — Duplicados y equivalencias

### 8.1 Niveles de coincidencia

| Nivel | Criterio | Nombre permitido |
|---|---|---|
| 0 | Mismo nombre | Homónimo; no implica igualdad |
| 1 | Mismo tamaño | Candidato a comparación |
| 2 | Mismo SHA-256 de archivo ZIP | Duplicado exacto del contenedor |
| 3 | Mismo hash de árbol crudo | Contenido extraído exacto |
| 4 | Mismo hash normalizado | Equivalente bajo normalización declarada |
| 5 | Mismo subconjunto fuente | Equivalente de fuentes, con extras distintos |
| 6 | Comportamiento/pruebas equivalentes | Equivalencia funcional acotada; no duplicado |

Sólo los niveles 2 y 3 permiten usar “duplicado exacto” sin calificador.

### 8.2 Sufijos `(1)`, `_1` y similares

Un sufijo de copia no demuestra duplicación. Cada variante se hashea. Si difiere:

- se calcula el delta;
- se buscan manifests y documentos internos;
- se ordena por evidencias, no automáticamente por el número del sufijo;
- se conserva el nombre original como dato de procedencia.

### 8.3 Árbol extraído frente a ZIP

Para vincular una carpeta y su ZIP se compara el hash de árbol después de aplicar únicamente la normalización de envolvente documentada. Las diferencias comunes se clasifican:

- el ZIP no contiene el propio ZIP pero la carpeta sí;
- la carpeta tiene archivos creados después de extraer;
- el ZIP conserva un directorio envolvente;
- atributos o directorios vacíos no se preservaron;
- el paquete está incompleto o la carpeta pertenece a otra revisión.

No se presupone que `Nombre` y `Nombre.zip` formen pareja exacta.

## 9. Fase F — Clasificación semántica de paquetes

### 9.1 Categorías

Cada paquete recibe una categoría respaldada por contenido:

- `FULL`: pretende ser autocontenido para el alcance declarado;
- `SOURCE_ONLY`: contiene fuentes y metadatos necesarios, excluyendo builds/binarios según contrato;
- `INCREMENTAL`: contiene un delta y declara o permite identificar una base;
- `CHANGED_FILES_ONLY`: variante incremental explícita;
- `CANDIDATE`: estado propuesto, no cierre final;
- `CERTIFIED`: afirma una certificación cuyo registro debe localizarse;
- `HANDOVER`: diseñado para transferir contexto y continuar trabajo;
- `DOCS`: documentación sin árbol completo;
- `BINARY/INSTALLER`: producto consumible, no fuente completa;
- `MIXED`: combinación deliberada o accidental;
- `EMPTY`: contenedor o árbol sin payload efectivo;
- `UNKNOWN`: aún no clasificable.

El nombre sólo propone una categoría. El catálogo la confirma o contradice.

### 9.2 Un FULL no siempre es limpio

El checkpoint FULL 50.5 incluye builds observados. Debe seguir clasificado como FULL si ésa era su intención, con la anomalía `CONTAINS_BUILD_ARTIFACTS`. El protocolo no elimina los extras para que el paquete parezca más limpio.

### 9.3 Un SOURCE_ONLY puede estar vacío

El checkpoint SOURCE_ONLY 50.11 observado vacío obtiene `EMPTY` además de conservar su etiqueta declarada. El FULL correspondiente, los handovers y el siguiente checkpoint se investigan para localizar el estado real, pero no se trasplanta contenido al paquete vacío.

### 9.4 Incrementales y su base

Un incremental sólo es reconstruible si tiene:

- base identificada por hash o regla inequívoca;
- operaciones de adición/modificación/eliminación;
- tratamiento explícito de renames;
- manifest resultante o forma de comprobarlo.

Si sólo contiene archivos cambiados sin lista de eliminaciones, se marca `PARTIAL_DELTA`: aplicarlo encima de una base puede dejar archivos obsoletos.

## 10. Fase G — Derivación de genealogía

### 10.1 Tipos de arista

| Arista | Significado |
|---|---|
| `derived-from` | El contenido demuestra que el hijo conserva una gran base común y añade/cambia un delta coherente. |
| `declared-successor` | Un documento dice que B continúa A. |
| `recovery-on` | Una entrega recupera trabajo aplicándolo sobre una base posterior o alternativa. |
| `consolidates` | Un paquete reúne varios cortes previos. |
| `candidate-of` | Un candidato antecede al paquete aceptado. |
| `source-variant-of` | Versión source-only de un full. |
| `contains-package` | Un contenedor incorpora otro paquete. |
| `exact-copy-of` | Coincidencia criptográfica exacta. |
| `probable-copy-of` | Coincidencia parcial; necesita revisión. |

### 10.2 Confianza de la arista

- **Alta:** hash/delta verificable más manifest o handover coherente.
- **Media:** gran continuidad de contenido y evidencia narrativa, con ambigüedades menores.
- **Baja:** nombre/timestamp o descripción sin suficiente confirmación material.
- **Contradicha:** una fuente afirma el vínculo pero el contenido lo hace imposible o apunta a otra base.

### 10.3 La numeración no decide el padre

El caso de referencia es:

```text
MacroA (31–34) -> Stage37
Stage37 -> Stage35 Recovery
Stage37 + Recovery35 -> Stage36 Recovery
Stage36 Recovery -> Stage38
```

La historia conservará el orden lógico de funcionalidades, pero el catálogo físico usará estas aristas. Nunca se forzará 34→35→36→37 sólo porque los números crecen.

## 11. Fase H — Deltas reconstruibles

### 11.1 Delta de contenido

Entre padre e hijo se calculan:

- archivos añadidos;
- retirados;
- modificados;
- idénticos;
- posibles renames, siempre como inferencia separada;
- cambios por extensión y subsistema;
- diferencia de manifests;
- diferencia de tests y documentación.

### 11.2 Rename no es igualdad de contenido suficiente

Dos archivos con el mismo hash y rutas distintas son candidatos a rename. Puede ser también una copia. Para llamarlo rename se revisa si desaparece la ruta antigua, aparece la nueva y los documentos o build scripts adoptan el nuevo nombre.

### 11.3 Delta pedagógico

El delta histórico puede ser demasiado grande para un capítulo. La obra lo divide en deltas pedagógicos, cada uno con:

- base exacta del capítulo anterior;
- patch o instrucciones completas;
- build y prueba mínima;
- resultado esperado;
- señalización de que el subcorte no sobrevivió históricamente;
- prueba final de convergencia con el checkpoint material.

## 12. Fase I — Evidencia de compilación y pruebas

### 12.1 Dos registros inmutables

Los resultados se separan en:

- `historical-results.tsv`: lo que consta en worklogs o documentos contemporáneos;
- `reproduced-results.tsv`: lo ejecutado durante la reconstrucción.

Una reproducción fallida no borra un 61/61 histórico; lo convierte en una discrepancia a investigar. Una reproducción verde tampoco autoriza decir que el paquete pasó en su fecha original.

### 12.2 Campos mínimos

- paquete y hash de árbol;
- comando exacto;
- directorio de trabajo;
- sistema operativo y arquitectura;
- compilador, linker, CMake, Ninja y LLVM;
- dependencias externas;
- variables de entorno relevantes sin secretos;
- suite o filtro;
- tests descubiertos, ejecutados, omitidos, fallidos y pasados;
- código de salida;
- duración;
- ruta del log completo;
- clasificación: histórico o reproducido.

### 12.3 Ausencia de herramienta

Si falta `xxd`, Wine, Windows, LLVM o una dependencia, el resultado será `NOT_RUN_ENVIRONMENT`, no `PASS` ni `COMPILER_FAILURE`. Si una comprobación usa una expresión regular defectuosa, se conserva primero el fallo original y luego una prueba corregida separada.

### 12.4 Validación estructural y ejecución

Se distinguen como gates independientes:

1. compila el compilador;
2. pasan unit tests;
3. emite IR;
4. LLVM verifica IR;
5. emite COFF;
6. COFF cumple estructura;
7. enlaza PE;
8. PE cumple estructura;
9. PE carga;
10. PE se ejecuta;
11. comportamiento y salida son correctos.

Stage19 puede sostener los niveles estructurales registrados, pero no se le atribuye el nivel de ejecución que no se realizó.

## 13. Fase J — Selección de milestones para recertificación

No es necesario compilar indiscriminadamente los 401 ZIPs. Primero se recertifican anclas que maximizan información:

1. `NativeSharp_snapshot_00_06` — primer árbol C++ observado.
2. `NativeSharp_delivery_07_10` — frontend conjunto.
3. Stages 11, 16, 18, 19 y consolidado 20 — saltos de IR a PE.
4. Stages 23 y 30 — apertura y cierre de la secuencia OOP temprana.
5. MacroA, Stage37, Recovery35, Recovery36 y Stage38 — genealogía no lineal.
6. Stages 41, 44 y final 45.12 — Core, genéricos y EH.
7. Stage46, un 47 full, 48.1 candidate/full, 49 y checkpoints representativos de 50.
8. Stages 51, 52, 53.4, 55 y 56 — runtime/biblioteca/correctness.

Si dos paquetes son duplicados exactos sólo se compila una copia y se registra que el resultado se aplica al mismo hash, no al nombre en abstracto.

## 14. Tratamiento de artefactos de build

Los builds se inventarían de nuevo en una reconstrucción limpia, pero los builds históricos pueden revelar:

- versiones y rutas del toolchain;
- comandos de link;
- outputs esperados;
- tests descubiertos;
- dependencias accidentales;
- símbolos y metadata;
- contaminación del paquete.

Por ello se catalogan en una capa separada. No participan en el hash “sólo fuentes”, pero sí en el hash crudo y en el informe de anomalías. Nunca se ejecutan automáticamente.

## 15. Tratamiento de productos vecinos

NSASM, NSLink, nsbind, bindings, ejemplos, Saru y pruebas de enlazadores pueden cumplir tres papeles:

1. **Fuente de requisitos:** exponen una necesidad que el compilador debe atender.
2. **Consumidor de contrato:** verifican ABI, metadata, headers, objetos o linking.
3. **Producto independiente:** poseen historia propia y no deben inflar artificialmente la genealogía interna de `nsc`.

El catálogo asignará `product` y `role`. Un ZIP de NSASM nunca se tratará como hijo de Stage56 sólo porque aparece después en la carpeta.

## 16. Integración de Markdown y chats

### 16.1 Markdown

Los 9,744 Markdown se deduplican por hash pero se conservan sus múltiples rutas. Después se indexan por:

- títulos y headings;
- fechas escritas;
- nombres de stages y versiones;
- rutas de código mencionadas;
- resultados de pruebas;
- decisiones normativas;
- handovers y TODOs.

Las revisiones con el mismo título pero distinto contenido se comparan por secciones. No se selecciona automáticamente la de mayor tamaño o la que tenga `(1)`.

### 16.2 Chats

Los chats se exportarán o leerán sin alterar su fuente. Para cada decisión relevante se conservará:

- fragmento mínimo necesario, evitando duplicar conversaciones enteras;
- contexto anterior y posterior suficiente;
- fecha y autor;
- vínculo al artefacto que implementa o contradice la decisión;
- estado: intención, orden, corrección, resultado o reflexión posterior.

El chat ayuda a explicar por qué. El árbol y los tests determinan qué quedó materializado.

## 17. Informes de calidad del catálogo

Cada ejecución del catalogador debe producir:

- total de entradas leídas y errores;
- bytes leídos;
- ZIPs válidos, corruptos, cifrados o peligrosos;
- manifests parseados y líneas rechazadas;
- grupos de duplicados exactos;
- equivalencias normalizadas;
- árboles sin pareja ZIP y ZIPs sin árbol extraído;
- incrementales sin base;
- paquetes vacíos;
- descripciones o versiones contradictorias;
- checkpoints con y sin evidencia de pruebas;
- cobertura de capítulos por fuentes.

Una ejecución se considera completa aunque encuentre errores, siempre que los reporte sin omitir silenciosamente entradas.

## 18. Revisión humana obligatoria

La automatización puede descubrir igualdad, diferencias y patrones. La decisión histórica sobre “qué ocurrió” requiere revisar al menos:

- toda arista de genealogía media o baja;
- toda contradicción entre handover y contenido;
- todo paquete candidato frente a final;
- todo incremental sin base explícita;
- toda regresión de versión;
- todo paquete vacío o contaminado;
- todo resultado de prueba que cambie de denominador;
- toda etapa lógica sin paquete nominal, como 20 o 45.8.

La revisión quedará firmada con fecha, criterio y fuentes consultadas.

## 19. Criterios de aceptación

El archivo histórico estará suficientemente catalogado para escribir la obra cuando:

- el 100% de archivos regulares tenga ruta, tamaño y SHA-256 o error explícito;
- el 100% de ZIPs tenga inventario de miembros o error explícito;
- cada manifest haya sido parseado o marcado con dialecto pendiente;
- cada supuesto duplicado tenga nivel de coincidencia;
- cada checkpoint narrado tenga una identidad de árbol;
- cada incremental tenga base o bloqueo documentado;
- cada resultado histórico esté separado de los reproducidos;
- cada capítulo 000–210 pueda consultar su familia de fuentes;
- ninguna operación haya modificado la colección original.

## 20. Qué permite reconstruir este protocolo

Con el catálogo terminado, una persona que no conoció el proyecto podrá responder de manera verificable:

- cuál es el primer código que realmente sobrevivió;
- qué estados del libro son reconstrucciones didácticas;
- qué paquete es padre material de otro;
- cuáles nombres son copias y cuáles revisiones reales;
- qué cambió exactamente entre etapas;
- qué se probó entonces y qué se volvió a probar hoy;
- dónde hay huecos honestos;
- qué estado debe usarse para reconstruir el siguiente capítulo;
- cuándo el compilador fue validado por productos externos;
- qué falta para llamar 1.0 a un artefacto y no sólo a una intención.

Ése es el estándar que necesita una historia capaz de sobrevivir incluso si vuelve a perderse el código fuente principal.
