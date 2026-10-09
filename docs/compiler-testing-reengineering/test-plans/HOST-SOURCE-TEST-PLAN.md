# Test plan — Host.Source

## Estado y estrategia

- **Clasificación:** `DISEÑO PROPUESTO` derivado de `IMPLEMENTACIÓN VIGENTE`.
- **Código de prueba creado:** ninguno.
- **Ejecución realizada:** ninguna.
- **Oráculo:** `components/HOST-SOURCE.md` y bytes preparados independientemente.

Cada fila es un caso lógico, no un ejecutable. Los casos deterministas de memoria son `PR`; filesystem y
carreras controladas son `NIGHTLY`; límites de memoria o escala son `WEEKLY`.

## Contratos

| Contrato | Superficie | Capa futura | Riesgo |
|---|---|---|---|
| CS-SRC-001 | `SourcePosition` | unitaria | base 0/1 incorrecta |
| CS-SRC-002 | construcción/ownership | unitaria | bytes o ruta transformados |
| CS-SRC-003 | índice de líneas | unitaria parametrizada | CR/LF/CRLF inconsistentes |
| CS-SRC-004 | offset a posición | unitaria parametrizada | EOF y columnas incorrectos |
| CS-SRC-005 | extracción de línea | unitaria parametrizada | terminadores visibles o línea perdida |
| CS-SRC-006 | lectura exacta | componente/filesystem | truncamiento, crecimiento o límite ignorado |
| CS-SRC-007 | registro y lookup | componente | IDs inestables y punteros mal usados |
| CS-SRC-008 | consultas por ubicación/rango | componente | slice o sentinel incorrecto |

## Casos de valores, construcción y ownership

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TS-SRC-001 | CS-SRC-001 | `SourcePosition{}` | línea 1, columna 1 | PR |
| TS-SRC-002 | CS-SRC-001 | agregado `{7,11}` | conserva 7 y 11 | PR |
| TS-SRC-003 | CS-SRC-002 | ID, ruta y buffer ASCII | accesores conservan los tres valores | PR |
| TS-SRC-004 | CS-SRC-002 | ruta Unicode representable | `path` original; `displayPath` igual a `path.string()` | NIGHTLY |
| TS-SRC-005 | CS-SRC-002 | buffer con NUL y `ff` | `buffer` y `size` cuentan todos los bytes | PR |
| TS-SRC-006 | CS-SRC-002 | mover strings al constructor | archivo posee valores después de morir las entradas | PR |
| TS-SRC-007 | CS-SRC-002 | vistas de buffer/ruta sin mutación | apuntan a bytes propiedad del archivo | PR |
| TS-SRC-008 | CS-SRC-002 | buffer artificial mayor a `UINT32_MAX` si el entorno lo permite | `size()` satura a `UINT32_MAX` | WEEKLY |

## Casos del índice de líneas

| ID | Contrato | Buffer | Oráculo | Frecuencia |
|---|---|---|---|---|
| TS-SRC-009 | CS-SRC-003 | vacío | un comienzo 0; una línea | PR |
| TS-SRC-010 | CS-SRC-003 | `abc` | una línea | PR |
| TS-SRC-011 | CS-SRC-003 | `a\nb` | comienzos 0,2; dos líneas | PR |
| TS-SRC-012 | CS-SRC-003 | `a\rb` | comienzos 0,2; dos líneas | PR |
| TS-SRC-013 | CS-SRC-003 | `a\r\nb` | comienzos 0,3; dos líneas | PR |
| TS-SRC-014 | CS-SRC-003 | `\n` | dos líneas, segunda vacía | PR |
| TS-SRC-015 | CS-SRC-003 | `\r` | dos líneas, segunda vacía | PR |
| TS-SRC-016 | CS-SRC-003 | `\r\n` | dos líneas, no tres | PR |
| TS-SRC-017 | CS-SRC-003 | `\n\r\r\n` | cuatro saltos lógicos según escaneo, cinco líneas | PR |
| TS-SRC-018 | CS-SRC-003 | NUL y bytes altos sin saltos | una línea; ningún byte especial crea línea | PR |

## Casos de posición y texto de línea

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TS-SRC-019 | CS-SRC-004 | offset 0 en `abc` | 1:1 | PR |
| TS-SRC-020 | CS-SRC-004 | offsets 1,2,3 en `abc` | 1:2, 1:3, 1:4 | PR |
| TS-SRC-021 | CS-SRC-004 | offset del LF en `a\nb` | primera línea, columna 2 | PR |
| TS-SRC-022 | CS-SRC-004 | offset inmediatamente después de LF | segunda línea, columna 1 | PR |
| TS-SRC-023 | CS-SRC-004 | bytes de CRLF por separado | CR pertenece a línea anterior; tras LF inicia siguiente | PR |
| TS-SRC-024 | CS-SRC-004 | offset exactamente EOF | posición EOF calculada | PR |
| TS-SRC-025 | CS-SRC-004 | offset mayor que tamaño | misma posición que EOF | PR |
| TS-SRC-026 | CS-SRC-004 | UTF-8 multibyte antes del offset | columna aumenta por bytes | PR |
| TS-SRC-027 | CS-SRC-005 | línea 0 | vista vacía | PR |
| TS-SRC-028 | CS-SRC-005 | línea posterior a `lineCount` | vista vacía | PR |
| TS-SRC-029 | CS-SRC-005 | `abc` línea 1 | `abc` | PR |
| TS-SRC-030 | CS-SRC-005 | líneas terminadas por LF, CR y CRLF | contenido sin terminador | PR |
| TS-SRC-031 | CS-SRC-005 | línea vacía intermedia | vista vacía pero número válido | PR |
| TS-SRC-032 | CS-SRC-005 | línea final vacía tras terminador | vista vacía y línea contada | PR |
| TS-SRC-033 | CS-SRC-005 | línea cuyo contenido termina en CR/LF repetidos por límites consecutivos | retira todos los terminadores contiguos del slice | PR |

## Casos de lectura

| ID | Contrato | Preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TS-SRC-034 | CS-SRC-006 | archivo vacío | `optional` con string vacío | PR |
| TS-SRC-035 | CS-SRC-006 | archivo binario con NUL | bytes exactos | PR |
| TS-SRC-036 | CS-SRC-006 | archivo de exactamente 64 MiB | éxito y tamaño exacto | WEEKLY |
| TS-SRC-037 | CS-SRC-006 | archivo de 64 MiB + 1 | fallo y texto de límite | WEEKLY |
| TS-SRC-038 | CS-SRC-006 | ruta inexistente | fallo y texto exacto de apertura con ruta | PR |
| TS-SRC-039 | CS-SRC-006 | ruta sin tamaño consultable | fallo y texto de tamaño | NIGHTLY |
| TS-SRC-040 | CS-SRC-006 | truncar después de `file_size` | fallo de lectura, sin buffer parcial | NIGHTLY |
| TS-SRC-041 | CS-SRC-006 | crecer después de `file_size` | `peek` detecta bytes extra y falla | NIGHTLY |
| TS-SRC-042 | CS-SRC-006 | `errorMessage` prellenado y lectura exitosa | éxito; texto previo permanece según contrato vigente | PR |
| TS-SRC-043 | CS-SRC-006 | `loadFile` con lectura fallida | `nullopt`; conteo de archivos no cambia | PR |
| TS-SRC-044 | CS-SRC-006 | `loadFile` exitoso | nuevo ID y snapshot exacto | PR |

## Casos de manager y consultas

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TS-SRC-045 | CS-SRC-007 | primer y segundo `addBuffer` | IDs 1 y 2; lookup recupera cada unidad | PR |
| TS-SRC-046 | CS-SRC-007 | dos rutas iguales | dos IDs distintos; no deduplica | PR |
| TS-SRC-047 | CS-SRC-007 | ID cero o mayor que N | `nullptr` | PR |
| TS-SRC-048 | CS-SRC-007 | ID válido | puntero con identidad/ruta/buffer correctos | PR |
| TS-SRC-049 | CS-SRC-007 | conservar puntero y provocar realocación | prueba documenta que no debe reutilizarse; sanitizador detecta uso indebido | NIGHTLY |
| TS-SRC-050 | CS-SRC-008 | ubicación con archivo inválido | posición 1:1, línea y ruta vacías | PR |
| TS-SRC-051 | CS-SRC-008 | ubicación válida con offset fuera de rango | posición y línea de EOF | PR |
| TS-SRC-052 | CS-SRC-008 | rango inválido por extremos/archivo | vista vacía | PR |
| TS-SRC-053 | CS-SRC-008 | rango válido vacío | vista vacía, sin acceso fuera de límites | PR |
| TS-SRC-054 | CS-SRC-008 | rango `[1,4)` en buffer conocido | bytes exactos 1..3 | PR |
| TS-SRC-055 | CS-SRC-008 | fin exactamente en tamaño | slice aceptado | PR |
| TS-SRC-056 | CS-SRC-008 | fin mayor que tamaño | vista vacía | PR |
| TS-SRC-057 | CS-SRC-008 | `getLineText` sobre CRLF y offset de segunda línea | contenido de segunda línea sin terminador | PR |
| TS-SRC-058 | CS-SRC-008 | `getPath` válido | representación `displayPath` congelada | PR |

## Oráculos y aislamiento

- Los buffers se construyen como secuencias explícitas de bytes; no mediante lectura de texto que normalice EOL.
- Las matrices CR/LF/CRLF comparan conteo, posición y slice, no sólo una de esas salidas.
- Las carreras de archivo usan coordinación determinista; un caso que no logra la mutación se marca no ejecutado,
  no aprobado.
- Los punteros y vistas se verifican bajo su lifetime permitido; no se convierte comportamiento indefinido en
  un contrato deseado.
- Los errores se comparan exactamente donde la cadena es observable y se separan por clase causal.

## Criterio de cierre futuro

Host.Source sólo podrá declararse verificado cuando los 58 casos estén implementados o justificados por fila,
las matrices de terminadores y filesystem estén completas, se pruebe integración mínima con Diagnostics/Lexer y
una revisión independiente confirme que no se omitió ningún símbolo o contrato de los cinco archivos.
