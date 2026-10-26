# Host.nsc — borde de proceso y observabilidad del compilador

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo diseño futuro explícito.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** `src/nsc/main.cpp`, 551 líneas y 24,169 bytes.
- **Inventario físico:** 18 símbolos: una clase y 17 funciones-like; incluye ambos entry points
  condicionales aunque el AST Windows sólo materializa `wmain`.
- **Resultado:** 20 contratos y 160 casos lógicos diseñados.

## 2. Frontera

`Host.nsc` transforma el entorno de proceso y `argv` en una llamada a `driver::Driver`. Es propietario de
aspectos que no pertenecen al compilador semántico: conversión Unicode de Windows, crash boundary, flags de
observabilidad, banner, captura de streams, progreso superior, supervisor LLVM, logs, time trace y failure
bundle.

No decide sintaxis, semántica, optimización ni enlazado. El exit code funcional procede del Driver o de la
frontera de crash; fallar al escribir telemetría sólo agrega una advertencia.

## 3. Entrada y crash boundary

En Windows, `wmain` convierte cada argumento UTF-16 a UTF-8 mediante dos llamadas estrictas a
`WideCharToMultiByte` con `WC_ERR_INVALID_CHARS`. Un argumento vacío permanece vacío; secuencias inválidas o un
resultado de longitud incoherente lanzan `runtime_error`.

Windows y otras plataformas llaman `runCompiler` dentro de `runWithCrashBoundary`. La diferencia es únicamente
la conversión de argv; ambos entry points preservan la misma contención de excepciones y diagnóstico de proceso.

## 4. Normalización de argumentos

`runCompiler` elimina primero el marcador privado del worker LLVM. Después delega en
`parseCompilerConsoleArguments` y activa `CompilerLanguageScope` durante todo el despacho. Un error global se
imprime como `nsc: error` y termina con `CommandLineError` antes de construir Driver.

La segunda pasada consume exclusivamente switches del borde de proceso:

- `--log=`, `--log-on-error=`, `--log-format=`;
- `--time-trace=`, `--save-failure-bundle=`;
- `--time-report`, `--quiet`, `--no-banner`, `--verbose`, `--no-error-log`.

Los demás argumentos conservan orden. `--time-report` se normaliza a `--time`; verbose agrega `--progress` y
`--time`; emisiones de producto agregan progreso por defecto si no se pidió explícitamente; time trace fuerza
medición temporal.

## 5. Clasificación y banner

`isCompilationInvocation` reconoce `build`, `run`, switches `--emit-*` y entradas `.ns`/`.nsproj`. Excluye
comandos de administración y consultas `check/dump/verify/print/write/compare/apply`. El banner humano sólo se
muestra cuando:

- no hay quiet;
- banner sigue habilitado;
- no es worker interno;
- no se solicitó JSON;
- la invocación es de compilación.

El formato de log sólo acepta `text` o `json`; cualquier otro valor produce error localizado de línea de
comandos y no invoca Driver.

## 6. Captura de salida

`TeeBuffer` duplica cada carácter hacia un stream real opcional y un buffer capturado. `overflow(EOF)` devuelve
`not_eof`; cualquier sink que falle hace fallar la operación. `sync` requiere éxito de todos los destinos no
nulos.

Stdout respeta quiet en su sink visible pero siempre se captura. Stderr siempre se refleja y captura. Driver,
banner y progreso usan los streams observados; después se hace flush antes de publicar artefactos.

## 7. Progreso superior y Driver

El parser de progreso acepta `--progress`, `--progress=<formato>` y `--progress-format=<formato>`. Un formato
inválido desactiva el progreso superior. Antes de Driver se emiten eventos `WorkStarted` para proyecto y target;
después se emite `WorkFinished` o `Failed` con nanosegundos transcurridos y conteo 1/1.

El target de detalle es el primer argumento normalizado. El proyecto es el primer argumento no-switch que
termina en `.nsproj`. `Driver::run` es la única autoridad del exit code funcional.

## 8. Supervisor LLVM

Cuando una traza LLVM requiere aislamiento en Windows y el proceso actual no es worker, se relanza el ejecutable
con un marcador privado y stderr capturado. El padre:

1. separa framing, diagnostics y pass log;
2. reproduce diagnostics;
3. advierte si no pudo leer completamente al hijo;
4. ante exit no cero, resume el último pase/unidad observado;
5. devuelve el exit code del hijo.

Si ni siquiera puede crear al hijo, informa una advertencia y ejecuta Driver localmente. Observar nunca crea una
nueva causa que impida compilar. El worker activa tracing y continúa; en plataformas no Windows se activa
directamente según los argumentos.

## 9. Publicación atómica

`publishPayload` y `publishLog` escriben `<destino>.tmp`, cierran, eliminan el destino anterior y renombran el
temporal. Ante error de rename eliminan el temporal. El resultado sólo es verdadero si el archivo final quedó
publicado.

El JSON de compilación usa schema `nativesharp-compilation-log`, version 1, exit, argumentos, stdout y stderr.
`jsonEscape` protege slash inverso, comillas, LF, CR y tab; descarta otros controles bajo `0x20`. El formato text
conserva cabecera versionada y secciones stdout/stderr.

## 10. Log automático y colisiones

Congelamiento 3.0 D-10: el log automático está desactivado por defecto. Sólo con `--error-log`, y salvo
`NSC_NO_ERROR_LOG=1` o un `--no-error-log` posterior, un fallo sin `--log` intenta escribir `error.log`,
`error1.log`, etc. en el directorio de trabajo, hasta un millón. Para evitar carreras, cada candidato reclama
primero un directorio `.claim`; sólo el dueño comprueba disponibilidad y publica. La claim se elimina siempre
después del intento.

Un log explícito se publica en éxito o fallo. `--log-on-error` sólo en fallo. Fallar al publicar cualquier log
acumula `logOk=false` y termina en advertencia; no altera el exit code del compilador.

## 11. Time trace y failure bundle

Time trace se renderiza desde stdout capturado y exit code, luego se publica atómicamente. Un bundle sólo se
intenta tras fallo y con path explícito. Requiere un argumento `.nsproj`; además extrae
`--library-reference-root` tanto unido como separado para reconstruir `ProjectLoadOptions`.

El writer recibe proyecto, salida, error, exit y argumentos normalizados. Una ausencia de proyecto o error del
writer produce advertencia localizada. Como los logs, bundle y time trace son observabilidad: su fallo no
reemplaza el resultado funcional.

## 12. Estados, errores y ownership

El flujo es lineal:

```text
ENTRY -> UTF8 -> GLOBAL_PARSE -> TRACE_MODE -> CAPTURE -> DRIVER -> OBSERVABILITY -> EXIT
```

Salidas tempranas válidas: error global, formato de log inválido y retorno del hijo supervisor. Streams y
buffers son stack-owned; vectores retienen strings/views durante la llamada; temporales de filesystem se limpian
en los caminos de fallo contemplados.

## 13. Complejidad

Parsing de argumentos, escaping y captura son lineales. La búsqueda de log automático es `O(k)` en colisiones
con cota un millón. Publicación escribe `O(payload)`. Conversión Unicode es lineal y realiza dos llamadas Win32
por argumento. El costo dominante normal pertenece a Driver; el borde agrega copia proporcional a stdout y
stderr porque conserva ambos streams completos en memoria.

## 14. Estrategia futura de pruebas

Los 20 contratos poseen ocho casos: siete PR y uno NIGHTLY. Helpers puros pueden probarse unitariamente; el flujo
de proceso necesita fixtures con entorno, cwd, filesystem, streams, child process y Driver controlados. Debe
demostrarse que observabilidad fallida nunca cambia el exit code funcional y que el marcador del worker no llega
al Driver.

El detalle está en `test-plans/HOST-NSC-TEST-PLAN.md`.

## 15. Reconstructibilidad y cierre

Quedan descritos entradas, salidas, contratos, estructuras, algoritmos, invariantes, estados, errores,
side-effects, ownership, plataforma, complejidad y observabilidad. Los 18 símbolos se trazan a 20 contratos y
160 casos; varios contratos de proceso comparten `runCompiler` como símbolo fuente porque esa función integra
las fronteras sin convertir sus ramas en contratos manuales.

`Host.nsc` queda `AUDITADO` y `RECONSTRUIBLE`. Esto no afirma ejecución, cobertura ni ausencia de defectos.
