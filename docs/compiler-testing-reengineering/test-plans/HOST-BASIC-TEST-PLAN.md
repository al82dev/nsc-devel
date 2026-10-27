# Test plan — Host.Basic

## Estado y regla de implementación futura

- **Clasificación:** `DISEÑO PROPUESTO` derivado de `IMPLEMENTACIÓN VIGENTE`.
- **Código de prueba creado:** ninguno.
- **Ejecución realizada:** ninguna.
- **Oráculo:** contratos documentados en `components/HOST-BASIC.md` y estándares explícitos de SHA-256.

Cada ID representa un caso lógico. En Fase 2 varios casos podrán parametrizarse dentro de un ejecutable
consolidado; el conteo no implica un ejecutable por fila.

Frecuencias:

- `PR`: rápida y determinista en cada cambio;
- `NIGHTLY`: filesystem, permisos, locks o matrices de plataforma;
- `WEEKLY`: límites costosos o interferencia externa controlada.

## Contratos y estrategia

| Contrato | Superficie | Capa futura | Riesgo principal |
|---|---|---|---|
| CB-BAS-001 | `FileId` | unitaria directa | confundir cero con un archivo real |
| CB-BAS-002 | `SourceLocation` | unitaria directa | validar indebidamente el offset |
| CB-BAS-003 | `SourceRange` | unitaria directa | aceptar cruces o rangos invertidos |
| CB-BAS-004 | eventos de compilación | unitaria directa | nombres/valores inestables |
| CB-BAS-005 | códigos de salida | contrato de componente | romper integración del proceso |
| CB-BAS-006 | identidad de release/build | componente/build | binarios indistinguibles o metadata inválida |
| CB-BAS-007 | SHA-256 en memoria | unitaria directa | padding, endian o ronda incorrectos |
| CB-BAS-008 | SHA-256 de archivo | componente/filesystem | tamaño/digest/error incoherentes |
| CB-BAS-009 | formato hexadecimal | unitaria directa | ancho o case incorrectos |
| CB-BAS-010 | clasificación de salida Windows | unitaria directa | diagnóstico no accionable |
| CB-BAS-011 | staging de artefactos | componente/filesystem | colisión, nombre interno obsoleto |
| CB-BAS-012 | preflight transaccional | componente/filesystem | destino alterado antes de validar el conjunto |
| CB-BAS-013 | commit multiarchivo | integración de componente | conjunto parcialmente publicado |
| CB-BAS-014 | rollback | integración de componente | pérdida de artefactos anteriores |
| CB-BAS-015 | escritura transaccional | componente/filesystem | truncamiento o publicación parcial |
| CB-BAS-016 | seed de stage | componente/filesystem | confundir cold path con error |

## Casos de valores y formatos pequeños

| ID | Contrato | Preparación y entrada | Oráculo exacto | Frecuencia |
|---|---|---|---|---|
| TB-BAS-001 | CB-BAS-001 | `FileId{}` | `isValid=false`, `value=0` | PR |
| TB-BAS-002 | CB-BAS-001 | `FileId(1)` | válido, valor 1 | PR |
| TB-BAS-003 | CB-BAS-001 | `FileId(UINT32_MAX)` | válido, valor conservado | PR |
| TB-BAS-004 | CB-BAS-001 | pares 0/0, 1/1, 1/2 | igualdad `true,true,false` | PR |
| TB-BAS-005 | CB-BAS-002 | ubicación por defecto | inválida, archivo cero, offset cero | PR |
| TB-BAS-006 | CB-BAS-002 | archivo válido con offset cero | válida y conserva ambos campos | PR |
| TB-BAS-007 | CB-BAS-002 | archivo válido con `UINT32_MAX` | sigue válida; offset no se valida aquí | PR |
| TB-BAS-008 | CB-BAS-002 | ubicaciones iguales/distintas | igualdad incluye archivo y offset | PR |
| TB-BAS-009 | CB-BAS-003 | rango por defecto | inválido | PR |
| TB-BAS-010 | CB-BAS-003 | extremos válidos e iguales | rango vacío válido | PR |
| TB-BAS-011 | CB-BAS-003 | comienzo menor que fin | válido y conserva extremos | PR |
| TB-BAS-012 | CB-BAS-003 | comienzo mayor que fin | inválido | PR |
| TB-BAS-013 | CB-BAS-003 | archivos diferentes | inválido aunque offsets estén ordenados | PR |
| TB-BAS-014 | CB-BAS-003 | un extremo inválido | inválido | PR |
| TB-BAS-015 | CB-BAS-004 | los ocho `CompilationEventKind` | nombres exactos documentados | PR |
| TB-BAS-016 | CB-BAS-004 | valor de enum fuera de rango | `unknown` | PR |
| TB-BAS-017 | CB-BAS-004 | `CompilationEvent{}` | kind `PhaseStarted`; vistas vacías; 0/0; 0 ns | PR |
| TB-BAS-018 | CB-BAS-004 | evento con todos los campos | cada campo conserva valor y duración | PR |
| TB-BAS-019 | CB-BAS-005 | los cuatro `ExitCode` | enteros exactos 0, 1, 2 y 70 | PR |
| TB-BAS-020 | CB-BAS-006 | constantes de la revisión | valores exactos de release, previous, language y stage | PR |
| TB-BAS-021 | CB-BAS-006 | header generado válido | exports iguales a los dos valores generados | PR |
| TB-BAS-022 | CB-BAS-006 | dos build IDs distintos | versión visible distingue construcciones | NIGHTLY |
| TB-BAS-023 | CB-BAS-006 | versión generada | SemVer válido cuya release es `kCompilerReleaseVersion` | PR |

## Casos SHA-256

Todos los vectores se comparan tanto como 32 bytes como mediante `sha256Hex`.

| ID | Contrato | Preparación y entrada | Oráculo exacto | Frecuencia |
|---|---|---|---|---|
| TB-BAS-024 | CB-BAS-007 | mensaje vacío | `e3b0c442...b855` | PR |
| TB-BAS-025 | CB-BAS-007 | ASCII `abc` | `ba7816bf...15ad` | PR |
| TB-BAS-026 | CB-BAS-007 | vector estándar de 56 bytes | digest publicado por NIST | PR |
| TB-BAS-027 | CB-BAS-007 | 55 bytes | padding cabe en el bloque actual | PR |
| TB-BAS-028 | CB-BAS-007 | 56 bytes | padding fuerza bloque adicional | PR |
| TB-BAS-029 | CB-BAS-007 | 63, 64 y 65 bytes | digests de autoridad externa para cada frontera | PR |
| TB-BAS-030 | CB-BAS-007 | bytes `00`, `80`, `ff` repetidos | digest binario, sin interpretación de texto | PR |
| TB-BAS-031 | CB-BAS-007 | un millón de `a` | vector estándar `cdc76e5c...2cd0` | NIGHTLY |
| TB-BAS-032 | CB-BAS-007 | misma entrada repetida | digest idéntico y sin estado residual | PR |
| TB-BAS-033 | CB-BAS-009 | digest todo cero | 64 caracteres `0` | PR |
| TB-BAS-034 | CB-BAS-009 | bytes `0f`, `10`, `ab`, `ff` | pares `0f10abff`; siempre minúsculas | PR |
| TB-BAS-035 | CB-BAS-008 | archivo vacío | éxito, tamaño 0, digest del vacío, error vacío | PR |
| TB-BAS-036 | CB-BAS-008 | archivo binario con NUL | tamaño y digest de todos los bytes | PR |
| TB-BAS-037 | CB-BAS-008 | ruta inexistente | `false`; error exacto contiene `cannot open file for SHA-256` y ruta | PR |
| TB-BAS-038 | CB-BAS-008 | ruta a directorio | fallo controlado; no se presenta digest nuevo como válido | NIGHTLY |
| TB-BAS-039 | CB-BAS-008 | archivo sin permiso de lectura | `false` y error de apertura/lectura con ruta | NIGHTLY |
| TB-BAS-040 | CB-BAS-008 | archivo alterado entre mediciones | resultado corresponde a los bytes realmente leídos; caso documenta carrera | WEEKLY |

## Casos de clasificación Windows

| ID | Contrato | Entrada | Oráculo exacto | Frecuencia |
|---|---|---|---|---|
| TB-BAS-041 | CB-BAS-010 | `ERROR_SUCCESS` | `None` / `none` | PR |
| TB-BAS-042 | CB-BAS-010 | filename range y buffer overflow | `PathLimit` / `path-limit` | PR |
| TB-BAS-043 | CB-BAS-010 | file/path not found, invalid drive, directory | `MissingComponent` | PR |
| TB-BAS-044 | CB-BAS-010 | access denied, privilege, write protect | `PermissionDenied` | PR |
| TB-BAS-045 | CB-BAS-010 | sharing, lock, mapped file | `FileLocked` | PR |
| TB-BAS-046 | CB-BAS-010 | disk full y handle disk full | `InsufficientSpace` | PR |
| TB-BAS-047 | CB-BAS-010 | código no enumerado | `IoError` / `io-error` | PR |
| TB-BAS-048 | CB-BAS-010 | valor inválido de `OutputFailureKind` | nombre fallback `io-error` | PR |

## Casos de staging y ciclo de vida

Cada caso usa un directorio temporal aislado y verifica también que no sobrevivan `.new`, `.old` ni staging
cuando el contrato exige limpieza.

| ID | Contrato | Preparación y entrada | Oráculo exacto | Frecuencia |
|---|---|---|---|---|
| TB-BAS-049 | CB-BAS-011 | `add` con destino vacío | `false`, error exacto, sin entrada | PR |
| TB-BAS-050 | CB-BAS-011 | primer `add` válido | stage bajo temp; basename idéntico; padre por índice | PR |
| TB-BAS-051 | CB-BAS-011 | dos destinos con mismo basename | stages distintos, ambos conservan basename | PR |
| TB-BAS-052 | CB-BAS-011 | destino Unicode | stage creado y ruta representable | NIGHTLY |
| TB-BAS-053 | CB-BAS-011 | colisión de nombre de staging | siguiente intento crea raíz distinta | NIGHTLY |
| TB-BAS-054 | CB-BAS-011 | agotar cien intentos | `false`, raíz limpia, error de creación | WEEKLY |
| TB-BAS-055 | CB-BAS-011 | `addStaged` con destino o stage vacío | `false`, error exacto | PR |
| TB-BAS-056 | CB-BAS-011 | `addStaged` con rutas no vacías | registra sin requerir existencia inmediata | PR |
| TB-BAS-057 | CB-BAS-011 | destruir sin commit | staging root y contenido eliminados best effort | NIGHTLY |
| TB-BAS-058 | CB-BAS-013 | commit sin entradas | `true`; segundo commit falla como ya comprometido | PR |
| TB-BAS-059 | CB-BAS-013 | segundo commit después de éxito normal | `false`, `artifact transaction was already committed` | PR |

## Casos de preflight, commit y rollback

| ID | Contrato | Preparación y entrada | Oráculo exacto | Frecuencia |
|---|---|---|---|---|
| TB-BAS-060 | CB-BAS-012 | stage ausente | fallo antes de tocar destinos; error nombra stage | PR |
| TB-BAS-061 | CB-BAS-012 | stage es directorio | mismo rechazo que stage no regular | PR |
| TB-BAS-062 | CB-BAS-012 | padre de destino ausente | se crea y el commit puede continuar | PR |
| TB-BAS-063 | CB-BAS-012 | destino existente es directorio | rechazo `destination-not-regular`; directorio intacto | PR |
| TB-BAS-064 | CB-BAS-012 | destino Windows es reparse point | rechazo `destination-not-regular` | NIGHTLY |
| TB-BAS-065 | CB-BAS-012 | destino Windows excede límite extendido | `path-limit`; ningún destino modificado | WEEKLY |
| TB-BAS-066 | CB-BAS-012 | espacio libre menor al stage | `insufficient-space`; ningún destino modificado | WEEKLY |
| TB-BAS-067 | CB-BAS-012 | falla copia/flush del último sibling | ningún destino final cambia; temporales limpiados | NIGHTLY |
| TB-BAS-068 | CB-BAS-013 | destino nuevo único | bytes publicados; stage no sustituye basename interno | PR |
| TB-BAS-069 | CB-BAS-013 | destino existente único | bytes nuevos y backup eliminado | PR |
| TB-BAS-070 | CB-BAS-013 | tres destinos nuevos | los tres visibles sólo tras preflight completo | PR |
| TB-BAS-071 | CB-BAS-013 | mezcla de destinos nuevos/existentes | todos reciben bytes correctos; sin residuos | PR |
| TB-BAS-072 | CB-BAS-013 | rutas Unicode y superiores a MAX_PATH en Windows | publicación correcta mediante rutas extendidas | NIGHTLY |
| TB-BAS-073 | CB-BAS-014 | segundo destino bloqueado después de instalar primero | ambos originales restaurados; `rollback-complete` | NIGHTLY |
| TB-BAS-074 | CB-BAS-014 | fallo al instalar primer destino | no altera entradas posteriores; rollback completo | NIGHTLY |
| TB-BAS-075 | CB-BAS-014 | backup existente no puede restaurarse | `rollback-incomplete`; se conserva error de commit | WEEKLY |
| TB-BAS-076 | CB-BAS-014 | fallo con destino originalmente ausente | destino parcialmente instalado se elimina | NIGHTLY |
| TB-BAS-077 | CB-BAS-014 | fallo tras dos instalaciones de tres | rollback inverso restaura las dos y preserva tercera | NIGHTLY |

## Casos de helpers transaccionales

| ID | Contrato | Preparación y entrada | Oráculo exacto | Frecuencia |
|---|---|---|---|---|
| TB-BAS-078 | CB-BAS-015 | bytes vacíos a destino nuevo | archivo vacío publicado, error vacío | PR |
| TB-BAS-079 | CB-BAS-015 | bytes binarios con NUL | contenido exacto, sin traducción | PR |
| TB-BAS-080 | CB-BAS-015 | texto con CR/LF y UTF-8 | bytes exactamente iguales al `string_view` | PR |
| TB-BAS-081 | CB-BAS-015 | no se puede abrir stage | `false`, error `cannot open staged artifact` | NIGHTLY |
| TB-BAS-082 | CB-BAS-015 | fallo de escritura/cierre | `false`, error `cannot finish staged artifact`, destino anterior intacto | WEEKLY |
| TB-BAS-083 | CB-BAS-016 | fuente ausente | `true`, error vacío, stage no creado | PR |
| TB-BAS-084 | CB-BAS-016 | fuente regular | stage creado con bytes exactos | PR |
| TB-BAS-085 | CB-BAS-016 | fuente o stage vacío | `false`, error exacto | PR |
| TB-BAS-086 | CB-BAS-016 | fuente es directorio | rama de plataforma documentada; no se publica archivo | NIGHTLY |
| TB-BAS-087 | CB-BAS-016 | stage en directorio ausente | directorio creado y copia exitosa | PR |
| TB-BAS-088 | CB-BAS-016 | inspección/copia/flush falla | `false`; error clasificado con ruta del stage | NIGHTLY |
| TB-BAS-089 | CB-BAS-013 | destino sin componente de directorio | publicado en el directorio actual; bytes exactos; sin residuos | PR |

## Oráculos y aislamiento requeridos

- Los hashes esperados deben provenir de vectores publicados o de una segunda implementación independiente,
  nunca de la misma función bajo prueba.
- Los casos de filesystem deben usar raíces únicas y comprobar bytes anteriores, bytes posteriores y residuos.
- Los fallos Win32 se provocan mediante handles, ACL, reparse points o una capa de filesystem inyectable futura;
  no se aceptan pruebas que pasen sólo porque el entorno no pudo crear el fallo.
- Las ramas no Windows se ejecutan únicamente en una plataforma que las compile; no se simulan afirmando paridad.
- Ninguna prueba depende del orden global de `transactionOrdinal`, sólo de ausencia de colisión dentro del caso.

## Criterio de cierre del componente en Fase 2

El componente sólo podrá considerarse verificado cuando:

1. los 88 casos estén implementados o exista una justificación explícita por fila;
2. las matrices Windows y no Windows aplicables publiquen resultados separados;
3. no sobrevivan temporales ni destinos parciales;
4. el oráculo SHA-256 sea independiente;
5. los formatos y textos contractuales permanezcan exactos;
6. una auditoría de cobertura automática posterior confirme rutas no previstas sin convertir ramas en contratos
   manuales.
