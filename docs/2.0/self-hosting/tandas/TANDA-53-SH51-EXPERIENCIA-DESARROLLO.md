# Tanda 53 — SH51: experiencia de desarrollo observable

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-401`–`NS2-SH-407` |
| Gate | `SH51` |
| Rama | `t53-staging` → `development-claude` |
| Milestone | `nativesharp-2.0-selfhost-milestone` |
| Commit de entrada | T52/SH50 `124b6a1e9` |
| Commit de cierre | este commit (`selfhost(tanda-53): make compilation failures observable`) |
| Fecha de cierre | 2026-09-22 |
| Baseline de cierre | acumulado en T56 por excepción terminal aprobada |

## 2. Objetivo y alcance

Hacer visible una compilación larga, conservar un expediente de fallo sin buscar texto en consola y
garantizar que NSC pueda consumir la metadata que él mismo publica. Incluye progreso, tiempos, logs,
diagnóstico estructurado, PDB/debugger y el defecto real por el que el escritor generaba
`Module::Namespace.Generic<T>.StaticMethod` pero el parser detenía la lectura en `::`.

No cambia sintaxis pública, ABI, NSLink, NSAsm ni las metadata históricas instaladas. Su regeneración
se hace con el compilador corregido; no se oculta mediante una reescritura de archivos existentes.

## 3. Diseño

`main` separa opciones de observabilidad de los argumentos del driver, captura stdout/stderr sin
cambiar sus códigos de salida y publica logs mediante temporal más rename. El driver escribe
NSMETA, la relee con `LibraryMetadataCodec`, verifica compatibilidad y analiza las superficies
pública y genérica. Sólo entonces conserva el archivo. El parser reconoce el prefijo de módulo en
la misma ruta de desambiguación usada por tipos genéricos y llamadas estáticas.

Invariantes: desactivar observabilidad no cambia artefactos; `--log-on-error` no publica en éxito;
un log nunca reemplaza el diagnóstico original; la metadata inválida se elimina; escritor y lector
comparten `Module::Namespace.Type<T>.Member`; los logs se publican transaccionalmente.

## 4. Inventario de archivos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `src/nsc/main.cpp` | modificar | opciones, captura y publicación de log/trace |
| `src/Driver/Driver.cpp` | modificar | ayuda y read-back obligatorio de NSMETA |
| `src/Parser/Parser.cpp` | modificar | receptor estático calificado por módulo |
| `tests/unit/Stage48_2ExternalTemplateTypeQualificationTests.cpp` | modificar | productor/consumidor externo de regresión |
| `selfhost/contracts/developer-experience.contract.json` | crear | contrato SH51 |
| `selfhost/bootstrap/verify_developer_experience.py` | crear | verificador dirigido |
| `cmake/VerifyNativeSharpDeveloperExperience.cmake` | crear | orquestación del gate |
| `CMakeLists.txt` | modificar | registro y dependencia acumulativa SH51 |
| roadmaps, expediente y evidencia SH51 | crear/modificar | cierre normativo |

## 5. Tipos, métodos y funciones

No se introduce un tipo público. `verifyPrecompiledGenericTemplateSource(metadata, error)` construye
la superficie genérica, siembra nombres nominales y exige lex/parse limpios.
`writeVerifiedLibraryMetadata(compilation, destination, error)` escribe, relee, valida versión,
superficie pública y templates; elimina el destino ante cualquier fallo. La ruta de
`Parser::looksLikeClosedGenericTypeReference()` salta un prefijo `Module::` y
`Parser::parsePrimaryExpression()` conserva ese prefijo para receptores estáticos no genéricos.
La capa de `main` analiza opciones de log, duplica los streams, serializa texto/JSON y renombra el
temporal; sólo toca archivos solicitados explícitamente.

## 6. Superficie observable

Opciones nuevas o congeladas: `--quiet`, `--verbose`, `--time-report`, `--time-trace=<file>`,
`--log=<file>`, `--log-format=text|json` y `--log-on-error=<file>`. El JSON registra argumentos,
stdout, stderr y `exit_status`. El contrato es
`nativesharp-selfhost-developer-experience-contract` v1.

## 7. Pruebas y resultados

| Prueba | Cobertura | Resultado |
|---|---|---|
| `nsc.selfhost.developer-experience` | 7 opciones, 3 logs, JSON, diagnóstico `nsc2`, 8 pruebas debug/PDB | aprobado |
| `nsc.stage48-2.external-template-type-qualification` | enum y genérico externos, llamada estática calificada, consumo sólo por LibraryReference | aprobado, 8 comprobaciones |
| probe con `C:\native-sharp\bin\nsc.exe` | producir NSMETA, releerla y compilar consumidor | aprobado |

Reporte dirigido SHA-256:
`90f4ea308cf3120520e3118b4792282bd2d17e64f9254751cd7d1ae8645e5763`.
Contrato: `7697a96a75e7920c526ebe0f3d73cf8453051035358ed5149c4584a6ba77f179`.
Verificador: `a4382c225d17f5c5e8c989496df88f8be95051afde79677fe0dcd5307fb24aeb`.

## 8. Comparación y baseline

Antes, el productor publicaba source template que el propio parser rechazaba en el primer `:`. La
corrección conserva la calificación completa y hace atómica la publicación. El baseline total de
T53 se difiere expresamente: T53–T55 ejecutan gates dirigidos y T56 ejecuta una única matriz total,
desde limpio y con ambos linkers. No se registra como aprobado el intento interrumpido de T53.

La instalación operativa quedó verificada con build `5300000000000001`, SHA-256
`1601696911311b6586e068042bb4889bcbcf8fa2777fd1ca32d8a875f6cbb4d4`; el probe temporal fue
eliminado después de pasar.

## 9. Riesgos, contenido e higiene

Los logs todavía no son un failure bundle completo de P02 y las metadata antiguas deben regenerarse
para beneficiarse del read-back. El commit no contiene binarios, objetos, logs, temporales ni el
reporte generado bajo `build-test-lldlinker`; sólo conserva el resumen estable SH51.

- [x] gate dirigido y regresión reales en verde;
- [x] instalación funcional comprobada por comportamiento;
- [x] roadmap y contrato actualizados;
- [x] baseline acumulativo reservado a T56;
- [x] commit único de tanda.
