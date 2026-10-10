# Análisis integral de errores — corrida `20261010-090333-82a4a13f`

## SCOPE LOCK de este análisis

- **Objetivo:** clasificar causalmente todos los resultados no exitosos de la última corrida completa de Fase 2.
- **Naturaleza:** auditoría estática y documental; no es una corrección funcional.
- **Autoridad de ejecución:** `A:\nsc-devel\_nscbuilder3\history\20261010-090333-82a4a13f`.
- **Configuración:** selección total, `Debug`, NativeSharp `O0`, perfil `Todos`.
- **Protegido:** código de producción de NSC, oráculos, manifiestos, suite histórica, seis cambios preexistentes y unidad `C:`.
- **Prohibido:** volver a ejecutar la batería, corregir producto, cambiar expectativas o convertir fallos en éxito.
- **Criterio de cierre:** los `555` resultados no exitosos quedan contabilizados, agrupados y separados entre causa confirmada, bloqueo derivado, defecto conocido y fallo todavía no diagnosticable.

## Identidad y evidencia

| Dato | Valor |
|---|---|
| Revisión | `c45f7f2f5d3252a149d8fa8a88b711bb368b7b64` |
| NSC | `A:\nsc-devel\build-clang\bin\nsc.exe` |
| SHA-256 NSC | `4b50dc5e42ca4d238c3ffcf4cb437588562185c331343167018d686a75ca54b1` |
| NSLink | `A:\native-sharp-3.0-baseline\bin\nslink.exe` |
| SHA-256 NSLink | `8de43e35e4e97c03b89ac84cbcb508555722eb08b8fefcaca468c4868a001476` |
| `metadata.json` | `E05612F5FFBE097DB0841B608201A73B5F85DAF90D65E082CCD6B1B3937C6935` |
| `results.json` | `5A6FD218B961BEEBFDFA888FE91001CEA67474305BC41126458EF7FF475B83B7` |
| `full.log` | `DA0A554E94A7CF8AA5614B1DC5EA5E06113969A70A48D553EA30A47F747F8FD4` |
| `summary.md` | `53CFD0A80D745C12E066AD1D96EEE6C392FBF0128D199EBC90836968AB62B4B3` |

La repetición posterior `20261010-110129-827bf0de` no sustituye esta corrida: contiene únicamente
`TF-DRIVER-027|Debug|NA`. Confirma que `CF-DRIVER-004` seguía fallando en `09389c625`, con los mismos binarios
NSC/NSLink, pero no aporta evidencia sobre los demás casos.

## Contabilidad cerrada

| Estado | Filas | Interpretación |
|---|---:|---|
| `PASS` | 5,140 | Contrato satisfecho en esta ejecución. |
| `FAIL` | 247 | Aserción requerida incumplida. |
| `INFRASTRUCTURE_ERROR` | 88 | El caso no produjo un veredicto contractual utilizable. |
| `BLOCKED` | 187 | Consumidor no ejecutado por fallo de un productor self-host. |
| `KNOWN_FAILURE` | 33 | Defecto de producto ya catalogado y reproducido. |
| **Total** | **5,695** | `5,140 + 247 + 88 + 187 + 33`; no hay filas perdidas. |

## Resultado contundente

La corrida no demuestra `555` defectos independientes.

- Los `88` errores de infraestructura se explican por **siete fronteras causales**.
- Los `187` bloqueos son consecuencia de **cinco productores**, todos afectados por una sola clase de ruta inválida.
- Los `33` defectos conocidos ya estaban clasificados; no son regresiones nuevas descubiertas por esta corrida.
- Los `247` fallos requeridos sí son trabajo abierto, pero el formato de evidencia actual guarda únicamente la
  etiqueta textual de la aserción. En `95` contratos no conserva los valores observado/esperado ni, para
  `ProjectCompilation`, `loadError` y diagnósticos. Por eso esta evidencia permite localizar la frontera exacta,
  pero no autoriza inventar una causa de producto o de prueba en todos los casos.

## Siete causas de infraestructura

### INFRA-01 — carga de gates Python sin su directorio importable

- **Afecta:** `36` casos BuildSystem: `T12B-00001..T12B-00030` y `T12B-00451..T12B-00456`.
- **Evidencia:** `ModuleNotFoundError: No module named 'component_gate_support'`.
- **Causa confirmada:** `ExecutionEngine._load_gate()` carga `run_buildsystem_gate.py` mediante
  `spec_from_file_location`, pero no hace importable el directorio hermano antes de ejecutar el módulo. El gate
  importa `component_gate_support` por nombre simple y falla antes de ejecutar su caso.
- **Clasificación:** defecto del adaptador Builder 3; no es un defecto de CMake, NSC ni BuildSystem.

### INFRA-02 — prerequisito de archivador ausente

- **Afecta:** `12` casos Packaging: `T12B-00457..T12B-00462` y `T12B-01069..T12B-01074`.
- **Evidencia:** `Packaging requires an archive executable`.
- **Causa confirmada:** `_run_packaging()` exige que `archive_executable` resuelva a un archivo antes de cargar el
  gate. La corrida no tenía archivador utilizable seleccionado o descubierto.
- **Clasificación:** prerequisito de infraestructura ausente; los oráculos Packaging no llegaron a ejecutarse.

### INFRA-03 — escala nocturna de Dominance rebasa el presupuesto

- **Afecta:** `11` casos: `TF-DOM-024`, `032`, `040`, `072`, `080`, `104`, `128`, `216`, `224`, `232`, `256`.
- **Evidencia:** todos terminan exactamente alrededor de `60.1–60.25 s` con `process timed out`.
- **Causa confirmada:** son las variantes nocturnas de escala; construyen cadenas de `2,048` o `4,096` nodos y
  Builder 3 les asigna la clase `medium = 60 s`. La terminación coincide con el límite, no con un veredicto del
  oráculo.
- **No demostrado:** el log no distingue si el costo excesivo está en el producto, en el oráculo independiente
  cuadrático o en ambos. Hace falta perfilado dirigido; aumentar el timeout sin medir no sería diagnóstico.

### INFRA-04 — oráculos C++ pierden sus precondiciones o la vida útil del modelo

- **Afecta:** `18` casos.
- **Frontend (`8`):** `TF-FRONT-043`, `409..412`, `414..416`.
- **HIR/MIR (`10`):** `TF-HIR-081..088`, `TF-MIR-186`, `TF-MIR-187`.
- **Causa confirmada:** `TestContext::expect()` sólo acumula texto; no aborta el caso. Los oráculos Frontend y MIR
  acceden después a `front()` o `blocks()` aunque el contenedor/puntero previo no haya satisfecho la precondición.
  En HIR, `lowerCase()` devuelve un nodo propiedad de un `FrontendCompilation` local que ya fue destruido.
- **Evidencia concreta:** `TF-FRONT-043` llama `ladder.front()` después de comprobar `!ladder.empty()` sin retorno;
  los casos macro llaman `expansions.front()` en la misma situación; `functionOracle` HIR desreferencia el nodo
  después de que el helper destruye su arena propietaria; MIR itera `function->blocks()` sin proteger el puntero.
- **Clasificación:** defecto confirmado del oráculo. Puede estar ocultando además un fallo real del producto, pero
  el aborto impide conocerlo.

### INFRA-05 — defectos conocidos de enum externo abortan antes del sobre de resultado

- **Afecta:** `TL-LANG-099` y `TL-LANG-250`.
- **Evidencia real de `host.log`:** aserción MSVC `array subscript out of range`.
- **Causa confirmada:** ambos casos pasan deliberadamente un valor enum externo a una API que indexa una tabla sin
  validarlo. El caso estaba diseñado como `KnownProductDefect`, pero el proceso Debug aborta antes de publicar JSON;
  Builder 3 sólo puede clasificarlo como infraestructura.
- **Clasificación:** defecto de producto ya identificado más una frontera de aislamiento/reporting insuficiente.

### INFRA-06 — validador de identidad usa un vocabulario incompleto

- **Afecta:** `TB-BAS-020..023`.
- **Evidencia:** los cuatro `results.json` anidados son coherentes y contienen exactamente una fila, pero sus
  `counts` incluyen `SKIPPED: 0`.
- **Causa confirmada:** `parse_identity_envelope()` construye `expected_counts` desde `RESULT_STATUSES`, que no
  contiene `SKIPPED`, y exige igualdad exacta de diccionarios. Rechaza por tanto un reporte válido producido por
  el controlador.
- **Clasificación:** incompatibilidad de esquema entre Builder 3 y su propio controlador.

### INFRA-07 — proyectos self-host usan rutas de fuente prohibidas por NSC

- **Productores afectados:** `TSH-00001`, `TSH-00188`, `TSH-00399`, `TSH-00849`, `TSH-00981`.
- **Proyectos:** `SelfHostBasic`, `SelfHostDiagnostics`, `SelfHostLexer`, `SelfHostSource`, `SelfHostTypes`.
- **Evidencia:** NSC rechaza `Compile Include="../../../../selfhost/..."` con
  `glob pattern cannot contain '.' or '..' segments`.
- **Causa confirmada:** los cinco `.nsproj` intentan incorporar fuentes externas mediante segmentos `..`; el
  cargador vigente de proyectos lo prohíbe.
- **Impacto derivado:** `187` consumidores quedan `BLOCKED`: Basic `26`, Diagnostics `27`, Lexer `52`, Source `47`,
  Types `35`.
- **Clasificación:** defecto de fixture/productor. Los `187` consumidores no fueron diagnosticados y no deben
  contarse como defectos independientes.

La suma de las siete fronteras es exacta:

```text
36 + 12 + 11 + 18 + 2 + 4 + 5 = 88 INFRASTRUCTURE_ERROR
5 productores -> 187 BLOCKED
```

## Los 247 `FAIL`

### Límite de lo que demuestra el artefacto

`TestContext::expect(bool, message)` conserva solamente `message`. Por ello, los `host.log` de `FAIL` dicen qué
invariante no se cumplió, pero omiten los operandos observados. No es posible decidir responsablemente, sólo con
esta corrida, si cada desacuerdo proviene de:

1. un defecto real del producto;
2. un oráculo que describe una capacidad no vigente o mal definida;
3. un fixture sintácticamente inválido;
4. una expectativa correcta cuyo valor observado simplemente no fue serializado.

### Frontera compartida de `ProjectCompilation`

`65` fallos comparten exactamente `valid project fixture must reach its requested frontend stage`. Afectan trece
contratos: `CF-FRONT-013`, `033`, `035`, `042`, `054`, `056..063`, cinco casos por contrato.

El helper `expectProjectStage()` sí recibe `loadError` y tiene acceso a `compilation.diagnostics()`, pero el mensaje
guardado no incluye ninguno. Queda confirmado que el proyecto temporal no alcanzó la etapa pedida; **no queda
confirmada la causa interna**. Las variantes de fuente vacía, sintaxis inválida y proyecto ausente siguen rutas
especiales y por eso no forman parte de estos 65.

Casos exactos:

- `CF-FRONT-013`: `TF-FRONT-097`, `098`, `100`, `103`, `104`.
- `CF-FRONT-033`: `TF-FRONT-257`, `258`, `260`, `263`, `264`.
- `CF-FRONT-035`: `TF-FRONT-273`, `274`, `276`, `279`, `280`.
- `CF-FRONT-042`: `TF-FRONT-329`, `330`, `332`, `335`, `336`.
- `CF-FRONT-054`: `TF-FRONT-425`, `426`, `428`, `431`, `432`.
- `CF-FRONT-056`: `TF-FRONT-441`, `442`, `444`, `447`, `448`.
- `CF-FRONT-057`: `TF-FRONT-449`, `450`, `452`, `455`, `456`.
- `CF-FRONT-058`: `TF-FRONT-457`, `458`, `460`, `463`, `464`.
- `CF-FRONT-059`: `TF-FRONT-465`, `466`, `468`, `471`, `472`.
- `CF-FRONT-060`: `TF-FRONT-473`, `474`, `476`, `479`, `480`.
- `CF-FRONT-061`: `TF-FRONT-481`, `482`, `484`, `487`, `488`.
- `CF-FRONT-062`: `TF-FRONT-489`, `490`, `492`, `495`, `496`.
- `CF-FRONT-063`: `TF-FRONT-497`, `498`, `500`, `503`, `504`.

### Índice exhaustivo por contrato

Los `247` fallos ocupan `95` contratos. La cardinalidad de cada fila suma exactamente el total:

- **Host.Compilation (11):** `CF-COMPILATION-013=1`, `027=8`, `042=1`, `048=1`.
- **Host.Dominance (1):** `CF-DOM-004=1`.
- **Host.Driver (18):** `CF-DRIVER-004=1`, `010=1`, `012=1`, `015=8`, `046=7`.
- **Host.Frontend (106):** `CF-FRONT-012=1`, `013=5`, `015=1`, `016=1`, `017=1`, `018=1`, `020=1`,
  `021=6`, `023=1`, `024=1`, `031=1`, `032=1`, `033=6`, `034=1`, `035=6`, `036=1`, `037=1`,
  `038=1`, `039=1`, `040=1`, `041=1`, `042=6`, `043=1`, `045=1`, `047=1`, `048=1`, `050=1`,
  `052=1`, `053=1`, `054=5`, `056=6`, `057=6`, `058=6`, `059=6`, `060=6`, `061=6`, `062=6`,
  `063=6`.
- **Host.HIR (5):** `CF-HIR-015=1`, `021=1`, `024=1`, `026=2`.
- **Host.Interop (16):** `CF-INTEROP-011=8`, `018=8`.
- **Host.LanguageService (14):** `CF-LANGUAGE-SERVICE-016=1`, `018=2`, `019=6`, `031=4`, `042=1`.
- **Host.MIR (16):** `CF-MIR-013=1`, `015=1`, `017=1`, `018=1`, `019=1`, `023=2`, `025=3`, `031=1`,
  `034=3`, `037=2`.
- **Host.Parser (8):** `CP-PAR-003=3`, `016=1`, `017=2`, `019=1`, `020=1`.
- **Host.Passes (14):** `CF-PAS-035=8`, `050=6`.
- **Host.Sema (33):** `CS-SEMA-012=1`, `013=1`, `014=2`, `015=6`, `016=2`, `017=4`, `018=2`,
  `019=2`, `020=1`, `021=6`, `023=3`, `025=1`, `027=1`, `029=1`.
- **Host.SSA (5):** `CF-SSA-017=1`, `025=1`, `034=1`, `035=1`, `037=1`.

### Lectura causal por subsistema

- **Parser (8):** discrepancias concretas en lookahead de tipos, cierre `>>=`, promociones duplicadas, variantes,
  extensión `impl` y `new` anidado. Cada caso está individualizado; faltan token/AST/diagnóstico observados.
- **Sema (33):** discrepancias en conteo/identidad de diagnósticos, completitud, layout Win64, `offsetof`, ABI C,
  conversiones enteras y volcado de conversiones. No forman una sola regresión demostrada.
- **HIR/MIR/SSA/Passes (40):** fallan contratos de lowering, verifier, promoción SSA, simplificación PHI y dump de
  constantes. Los mensajes prueban el invariante roto, pero no publican el nodo/opcode/veredicto observado.
- **Compilation (11):** ocho filas comparten el contrato de primer error XML; las otras tres cubren owner/metadata,
  materialización y selección de SDK.
- **Frontend (106):** `65` se detienen en la frontera `ProjectCompilation`; los `41` restantes alcanzan un oráculo
  de catálogo, genéricos, method bodies, lowering o dump y encuentran el modelo esperado vacío o diferente.
- **Interop (16):** dos contratos completos de ocho variantes fallan en identidad ABI y clasificación de diff;
  esto apunta a una discrepancia sistémica de modelo/oráculo, no a dieciséis causas independientes.
- **Driver (18):** cinco contratos: política instalada, time trace, progreso, cancelación y guía de migración.
  `TF-DRIVER-027` volvió a fallar aisladamente; los demás no se repitieron.
- **LanguageService (14):** formato, asm, símbolos de workspace y snapshot semántico; faltan texto/edits/símbolos
  observados en el resultado.
- **Dominance (1):** el verifier no produjo el veredicto esperado para metadata nula o inconsistente; el resultado
  no indica cuál variante de corrupción fue aceptada/rechazada incorrectamente.

## Los 33 `KNOWN_FAILURE`

Estos casos ejecutaron su oráculo y reprodujeron el defecto esperado. No deben convertirse en `PASS`, pero tampoco
son fallos nuevos de infraestructura:

- **Host.CFG (1):** `TF-CFG-076`.
- **Host.Dominance (1):** `TF-DOM-142`.
- **Host.Language (5):** `TL-LANG-019`, `035`, `121`, `230`, `262`.
- **Host.LanguageService (1):** `TF-LANGUAGE-SERVICE-304`.
- **Host.Lexer (1):** `TL-LEX-075`.
- **Host.Parser (20):** `TP-PAR-063`, `069`, `098`, `103`, `306`, `309`, `316`, `327`, `334`, `355`,
  `356`, `358`, `400`, `405`, `406`, `422`, `431`, `450`, `476`, `511`.
- **Host.Sema (4):** `TS-SEMA-106`, `110`, `246`, `317`.

`TL-LANG-099` y `TL-LANG-250` no aparecen aquí porque abortaron antes de producir el sobre; se contabilizan en
`INFRA-05`.

## Orden de corrección derivado del análisis

1. Corregir primero `INFRA-01`, `04` y `06`, porque impiden obtener evidencia fiel aun cuando el producto termine.
2. Resolver el contrato de prerequisito de `INFRA-02`; sin archivador no existe ejecución Packaging.
3. Reescribir las rutas de los cinco proyectos de `INFRA-07` sin adaptar NSC ni usar artefactos viejos.
4. Separar escala y costo en `INFRA-03`; medir producto y oráculo antes de decidir timeout o algoritmo.
5. Mantener `INFRA-05` como defectos conocidos, pero aislar el aborto para que el resultado se publique como
   `KNOWN_FAILURE` con evidencia.
6. Para los `247 FAIL`, ampliar primero la evidencia dirigida de cada oráculo con valores observado/esperado y,
   en `ProjectCompilation`, error de carga y diagnósticos. Después auditar el contrato contra la capacidad vigente.
7. Sólo cuando un contrato sea correcto y el valor observado sea incorrecto se abre una corrección de producto.
   Si el contrato inventa una capacidad no vigente, se corrige la prueba; nunca se modifica NSC para hacerla pasar.

## Estado

- **Inventariados:** `555/555` resultados no exitosos.
- **Omitidos:** `0`.
- **Desconocidos:** `0` filas; sí existen causas internas no diagnosticables en los `247 FAIL` por insuficiencia
  del sobre de evidencia, registradas expresamente arriba.
- **Producto modificado:** no.
- **Pruebas ejecutadas nuevamente:** no.
- **Cierre funcional:** no; este documento cierra la clasificación de la corrida, no los defectos.
