# Host.Diagnostics — inventario semántico y contrato reconstructivo

## Estado, alcance y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE` y `HECHO COMPROBADO`.
- **Revisión:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Componente documental:** `Host.Diagnostics`.
- **Universo cerrado:** ocho archivos, 764 líneas y 22,929 bytes.
- **Fuentes:** `include/nativesharp/Diagnostics/**` y `src/Diagnostics/**`.
- **Validación ejecutada:** ninguna; esta fase es de auditoría y diseño.

## 1. Propósito y frontera

`Host.Diagnostics` define el vocabulario, almacenamiento, acumulación, combinación y representación textual de
los problemas producidos por el compilador. Su frontera contiene:

1. 74 identificadores estables;
2. cuatro severidades;
3. un diagnóstico propietario con rango primario, etiquetas y notas;
4. un motor con conteos y presupuesto de errores;
5. combinación de motores locales;
6. renderizado con ruta, coordenada, línea, marcador y resumen de supresión.

No decide cuándo una construcción del lenguaje es válida. Recibe diagnósticos de otros componentes y conserva
o presenta su información.

## 2. Archivos y responsabilidades

| ID | Archivo | Responsabilidad |
|---|---|---|
| PF-000115 | `include/nativesharp/Diagnostics/Diagnostic.h` | modelo propietario de diagnóstico |
| PF-000116 | `include/nativesharp/Diagnostics/DiagnosticEngine.h` | acumulación, límite, merge y consultas |
| PF-000117 | `include/nativesharp/Diagnostics/DiagnosticId.h` | catálogo y nombres machine-facing |
| PF-000118 | `include/nativesharp/Diagnostics/DiagnosticRenderer.h` | API de representación |
| PF-000119 | `include/nativesharp/Diagnostics/DiagnosticSeverity.h` | severidades y nombres |
| PF-000651 | `src/Diagnostics/Diagnostic.cpp` | construcción y accesores |
| PF-000652 | `src/Diagnostics/DiagnosticEngine.cpp` | estado, supresión y orden canónico |
| PF-000653 | `src/Diagnostics/DiagnosticRenderer.cpp` | formato visible y caret |

## 3. Vocabulario estable

### 3.1 Severidades

`DiagnosticSeverity` contiene `Note`, `Warning`, `Error` y `Lint`. Sus nombres exactos son `note`, `warning`,
`error` y `lint`. Un valor fuera del enum se representa como `error`.

El orden de declaración no es el orden del merge. La combinación usa explícitamente:

`Error < Warning < Lint < Note`.

### 3.2 Identificadores

`DiagnosticId` contiene 74 valores en la revisión congelada. Los primeros 69 se representan con el nombre del
enumerador. Los cinco finales se representan como códigos cortos:

| Enumerador | Nombre machine-facing |
|---|---|
| `NS3010InvalidCompilationStage` | `NS3010` |
| `NS3020InvalidValueCategory` | `NS3020` |
| `NS3030MissingEffectContext` | `NS3030` |
| `NS3049DeferredLanguageSurface` | `NS3049` |
| `NS3099InvalidConformanceManifest` | `NS3099` |

`isNativeSharp3Diagnostic` es `true` sólo para esos cinco. El nombre refleja procedencia histórica; en este
componente no selecciona edición ni rechaza código. Sólo controla si el renderer imprime el código entre
corchetes. Un valor desconocido se nombra `UnknownDiagnostic` y no pertenece al subconjunto.

## 4. Modelo propietario `Diagnostic`

`DiagnosticLabel` combina un `SourceRange` y un mensaje propio. `Diagnostic` conserva por valor:

- ID y severidad;
- ubicación principal;
- rango primario opcional;
- mensaje principal;
- vector de etiquetas secundarias;
- vector de notas.

El constructor mueve strings y vectores, pero copia valores de ubicación/rango. Los accesores devuelven valores
pequeños o referencias constantes. La instancia no depende de la vida de strings del productor.

El constructor no valida coherencia entre `location` y `range`, validez de rangos ni archivos de etiquetas. Los
productores normales del motor mantienen esa coherencia; un llamador directo puede crear una combinación
inconsistente que el renderer no corrige.

## 5. Motor y máquina de estado

### 5.1 Estado inicial

El límite por defecto es 100. Un límite solicitado de cero se normaliza a uno. El motor comienza con vector y
conteos vacíos, cero suprimidos y `errorLimitReached = false`.

### 5.2 Reporte

Existen tres rutas:

- ubicación: rango ausente;
- rango: ubicación igual a `range.begin()`;
- estructurada: rango primario, etiquetas y notas.

Antes de insertar, todas llaman `suppress`. Si no se suprime, agregan exactamente un `Diagnostic`. `Error`
incrementa errores y marca el límite al alcanzar `errorLimit`; `Warning` incrementa warnings; `Lint` incrementa
lints; `Note` no tiene contador propio. Los conteos representan diagnósticos almacenados de esas severidades.

### 5.3 Presupuesto y supresión

Una vez alcanzado el límite de errores, `suppress` rechaza **todo diagnóstico posterior**, sin importar su
severidad, e incrementa `suppressedDiagnosticCount`. El error que alcanza el límite sí se almacena. El parámetro
de severidad de `suppress` no se usa.

La transición es:

`aceptando -> aceptar error límite -> límite alcanzado -> suprimir todos los reportes posteriores`.

### 5.4 `append`

`append` reproduce un diagnóstico a través de las APIs de reporte para aplicar una sola vez el presupuesto del
motor receptor:

- si hay etiquetas o notas usa reporte estructurado;
- si además no había rango, sintetiza el rango vacío `[location,location)`;
- si sólo hay rango usa el overload de rango;
- en otro caso usa ubicación.

Por ello, anexar un diagnóstico estructurado sin rango cambia `range` de ausente a presente/vacío. Mensajes,
etiquetas y notas se copian al receptor.

### 5.5 Merge

`mergeDeterministically` reúne punteros a diagnósticos de todos los shards, suma sus conteos de suprimidos,
ordena y luego llama `append`. La clave es:

1. ubicaciones válidas antes que inválidas;
2. valor de `FileId`;
3. offset inicial;
4. offset final o inicial si no hay rango;
5. severidad en orden Error/Warning/Lint/Note;
6. valor entero de ID;
7. mensaje.

El receptor conserva diagnósticos previos y aplica su límite al conjunto combinado. Después suma los
suprimidos de los shards, incluso si el receptor también suprimió entradas durante el merge.

**Riesgo vigente:** la clave no incluye etiquetas ni notas y se usa `std::sort`, no `stable_sort`. Dos
diagnósticos que empatan en toda la clave pero difieren sólo en datos secundarios no tienen un orden relativo
especificado. Por ello la implementación garantiza orden canónico entre claves distintas, pero no demuestra
independencia del orden de llegada para ese empate. Se documenta; no se corrige en Fase 1.

Complejidad: recolectar `O(D)`, ordenar `O(D log D)`, copiar mediante append `O(D + contenido)` y memoria `O(D)`.

### 5.6 Consultas y clear

Los accesores exponen vector, conteos, límite, bandera y suprimidos. `hasErrors` equivale a `errorCount != 0`.
`clear` vacía diagnósticos y pone a cero todos los conteos y la bandera, pero conserva `errorLimit_`.

## 6. Renderizado

### 6.1 Encabezado

Cada diagnóstico comienza:

`<ruta>:<línea>:<columna>: <severidad>[<código opcional>]: <mensaje>\n`

La ruta fallback es `<source>`. El código entre corchetes sólo aparece para los cinco IDs que reconoce
`isNativeSharp3Diagnostic`; los otros 69 y un ID desconocido no imprimen código.

### 6.2 Línea y marcador

Después del encabezado consulta la línea. Si ésta es vacía y la columna no es uno, retorna inmediatamente. En
ese camino no imprime línea, caret, etiquetas secundarias ni notas.

En los demás casos imprime cuatro espacios, texto de línea, nueva línea y cuatro espacios. Antes del caret
replica tabuladores de la línea y usa espacios para otros bytes. El marcador siempre tiene al menos `^`.

Si existe rango, su final cae en la misma línea y su offset es mayor que el inicial, la longitud es
`max(1, end.column - start.column)`; los caracteres restantes son `~`. Rangos multilínea conservan longitud uno.
La longitud no se limita al texto visible.

Un `Diagnostic` construido directamente puede tener `location` distinta de `range.begin()`. Si aun así el fin
cae en la misma línea pero su columna es menor, la resta unsigned puede producir un marcador enorme. El motor
evita el caso al derivar ubicación del rango; la API pública del modelo no lo impide.

### 6.3 Etiquetas, notas y resumen

Cada etiqueta secundaria se imprime sin línea/caret:

`<ruta>:<línea>:<columna>: note: <mensaje>\n`

Cada nota se imprime `note: <texto>\n`. `renderAll` conserva el orden del vector. Si hubo supresiones añade:

`nsc: error: diagnostic limit reached after <errores> errors; <N> additional diagnostics suppressed\n`

La palabra `errors` no singulariza.

## 7. Ownership, lifetime y concurrencia

- `Diagnostic` posee todos sus textos y vectores.
- `DiagnosticEngine` posee diagnósticos copiados/movidos internamente.
- Durante `merge`, los punteros sólo viven mientras los shards y sus vectores no cambien; el método no muta los
  shards.
- `DiagnosticRenderer` guarda referencia no propietaria a `SourceManager`; éste debe sobrevivir y no mutar de
  forma incompatible mientras se renderiza.
- No se declara seguridad concurrente para reportar o limpiar un mismo motor. El patrón previsto es un motor
  local por worker y merge posterior.
- La salida puede lanzar por el stream; `render`/`renderAll` no son `noexcept`.

## 8. Dependencias e integración

| Productor/consumidor | Contrato |
|---|---|
| Host.Basic | ubicaciones y rangos byte |
| Host.Source | ruta, posición y línea visibles |
| Lexer/Parser/Sema/IR/Driver | crean IDs, mensajes, rangos, etiquetas y notas |
| workers del frontend | producen shards que se combinan |
| CLI/IDE/LSP | consumen texto, códigos y orden machine-facing |
| `ostream` | destino secuencial de renderizado |

Cambiar nombres, orden, sentinels o conteos afecta integraciones aunque no cambie la aceptación del lenguaje.

## 9. Casos límite y hallazgos

- límite cero se convierte en uno;
- el error que alcanza el límite se conserva; todo lo posterior se suprime;
- `Note` no tiene contador;
- `append` puede materializar un rango vacío antes ausente;
- el merge acumula en vez de reemplazar el receptor;
- empate de clave con secundarios diferentes carece de orden estable garantizado;
- línea vacía y columna distinta de uno elimina también notas/labels del texto final;
- una línea vacía en columna uno sí imprime línea vacía y caret;
- rangos multilínea marcan un solo carácter;
- un modelo incoherente construido directamente puede desbordar longitud de marcador;
- `clear` conserva configuración de límite.

## 10. Evidencia histórica observada

La suite congelada contiene pruebas unitarias del engine/renderer y muchos consumidores que comparan errores.
No se modifica. La nueva suite debe separar vocabulario, estado, merge, formato y fallos para que un solo
ejecutable monolítico no oculte el contrato que dejó de ejercitarse.

## 11. Guía de reimplementación

1. congelar enums y nombres machine-facing;
2. implementar el valor propietario `Diagnostic`;
3. implementar reporte/conteo con la transición exacta de límite;
4. implementar append conservando la particularidad del rango estructurado;
5. definir y probar una clave total de merge, incluyendo una decisión explícita para empates;
6. implementar encabezado, línea, tabs, caret, labels y notes;
7. integrar SourceManager y consumidores;
8. comprobar byte a byte todos los formatos.

Una reimplementación puede corregir riesgos sólo mediante una decisión autorizada y una actualización de
contrato; no debe corregirlos silenciosamente dentro de una migración de pruebas.

## 12. Respuestas de reconstructibilidad

| Pregunta normativa | Respuesta y sección |
|---:|---|
| propósito/problema | Sección 1 |
| entradas/salidas | Secciones 3 a 6 |
| contratos/invariantes | Secciones 3 a 6 y 9 |
| estructuras/algoritmos | Secciones 4 a 6 |
| errores/estados | Secciones 5, 6 y 9 |
| ownership/lifetime | Sección 7 |
| interacciones/formatos | Secciones 6 y 8 |
| complejidad | Sección 5.5 |
| alternativas/evolución | No verificadas históricamente; riesgos vigentes documentados |
| evidencia | Sección 10 y test plan separado |
| reimplementación | Sección 11 |

## 13. Resultado de reconstructibilidad

`RECONSTRUIBLE` para la revisión congelada, incluyendo sus sentinels y riesgos observables. La segunda revisión
independiente está en `audits/TANDA-03-HOST-DIAGNOSTICS-AUDIT.md`. No se implementaron ni ejecutaron pruebas.
