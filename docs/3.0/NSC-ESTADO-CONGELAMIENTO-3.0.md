# NativeSharp 3.0 — Estado para el congelamiento

- **Fecha**: 2026-10-27.
- **Rama**: `development`. Hasta el elemento 135a se trabajó en `development-release-candidate`, que entró por el
  merge `15dc778d5`.
- **Base de la campaña**: `63aba0be4`.
- **Revisión descrita**: `15dc778d5` más el commit del elemento 139, que actualiza este documento.
- **Commits desde la base**: 85 sin contar merges antes de esta versión. Dos de ellos entraron por merge:
  - `4da911c53` revierte la época NSMETA de P8;
  - `2796870df` es la primera versión del elemento 115, reemplazada por `e64631180`.
- **Estado**: **IMPLEMENTADO CON VALIDACIÓN FOCAL**. No está congelado ni certificado; los gates finales de abajo
  siguen pendientes.
- **Tracking**: plan de decisiones de 154 elementos en el proyecto NSC-IDE
  (`NSC-PLAN-DECISIONES-CONGELAMIENTO-3.0.md`). Este documento es su elemento 132, actualizado en el 137, en el
  135a y en el 139.
- **Registro de decisiones**:
  - Este documento reemplaza la instantánea de `NSC-DECISIONES-VIGENTES-CONGELAMIENTO-3.0.md`. Esa instantánea
    (§2) y sus pendientes (§6) son de `403b06641`.
  - Lo hecho después está en la tabla de contratos de abajo y en
    `docs/3.0/decisiones-congelamiento/34-*` a `59-*`.
- **Contrato de vida**: `docs/3.0/language/known-life-sources.md` (elemento 137) fija la garantía sobre fuentes de
  vida conocida y sus límites exactos.

## 1. Afirmaciones retiradas de la versión anterior

La versión del 2026-10-25 (`9b224f4df`, actualizada en `aa80795b4`) afirmaba lo siguiente; la auditoría
(`NSC-AUDITORIA-CONGELAMIENTO-3.0.md`, A-07 y A-09) y las decisiones posteriores lo retiran:

- Plan de 108 elementos terminado; compilador validado `73b03a394` → Retirada. El trabajo siguió hasta `1df8e86e3`.
- «Cada punto tiene sonda, corrección, pruebas nuevas y un commit propio» → Excesiva. `ea0da601c` mezcla al menos once
  hipótesis; 12 de los 17 primeros commits no tienen documento editorial.
- «`checkall`: 349 proyectos, 75 fallas, idénticas a la base previa» → Falsa. La «base» se tomó después de migrar la
  batería; no hay medición previa a C-01.
- «`casecheck`: 52 casos, 0 fallas» → Sólo vale sobre el árbol migrado; sobre los casos históricos eran 36 fallas.
- C-01: «el `catch` es dueño de la excepción» → Causaba uso después de liberar (D-02). Lo reemplazan DEC-10 y P10.
- C-02: despacho por estado «al inicio de la sentencia» → Corrección incompleta (D-01). Corregida en `ba91ef419`.
- C-05: las nueve acciones CFG/dominancia/SSA/opt salen con código 2 → Reemplazada por DEC-05: inspeccionan el LLVM del
  pipeline 3.x.
- C-12: `delete items[i]` y `move(arr[i])` dejan el slot en cero → Era un defecto (D-03): el cero se destruía. DEC-11 da
  a cada slot estado vivo/muerto.
- Limitación 2: «el destructor debe tolerar el valor por defecto» → Era el mismo defecto D-03, ya corregido.
- P-3: `delete[]` de un local `owned T[]` es error → Reemplazada por DEC-06: `delete[]` consume el dueño.
- P-5: un símbolo ausente de una DLL del sistema se rechaza al compilar → Reemplazada por DEC-13: la validación es
  opcional (`--validate-system-imports`).
- Anexos A y B (98 casos y 168 sondas) → Medidos en `73b03a394` sobre el árbol migrado. Quedan en la historia
  (`aa80795b4`), no como evidencia del estado actual.

## 2. Contratos vigentes

- **C-08 / DEC-09** (`6758083f5`, `7f6214d00`; doc 32). `IAllocator` con una sola firma (tamaños `long`, alineación
  explícita); la firma 3.0.4 se rechaza con diagnóstico de migración.
- **C-06 / DEC-07/08** (`73150c308`, `30959b2a8`, `1bde9d387`; docs 04, 05). `NSLANGSURFACE 3.0` publica lo
  implementado, incluidas las reglas move-only de C-11 y C-14.
- **C-07** (`7b4f91771`). `NativeSharp.Core.IDisposable`; `using` dispone una vez por salida.
- **C-01 / DEC-02** (`ea0da601c`, `8632a809e`, `e64631180`, `54753728f`; docs 02, 43, 52). Todo `new` alcanza un dueño.
  Es error un `new` en un local, un parámetro, un retorno o un campo no dueños, en una expresión descartada y como
  receptor de un método. `throw` transfiere la excepción.
- **C-02 / DEC-03/04** (`ea0da601c`, `6d941cfcc`, `8c1559649`; docs 09, 10). Un productor conocido que puede suspender
  no admite `await` síncrono. Un origen desconocido se permite; si queda `Pending` tras el único poll, hace pánico.
- **D-01** (`ba91ef419`, `6a5dc1a10`, `485edb50e`, `b1225b9be`, `cc3cba07f`, `e8dd0bbac`; docs 14, 39–42). Un `await`
  reanudado no repite los operandos de su sentencia; tampoco en `await F(await G())`, `loop`, `match` ni operandos antes
  de un `loop`.
- **C-05 / DEC-05** (`2e154f127`, `30fe9b7e7`; doc 11). Un `.ns` es un proyecto implícito del pipeline 3.x; las acciones
  CFG/dominancia/SSA/opt inspeccionan su LLVM.
- **C-11 / DEC-11** (`4d6237d84`, `b1f1a9699`; doc 15). Structs con destructor o campos `owned`: move-only, destrucción
  por scope y estado vivo/muerto por slot de arreglo.
- **C-11 / temporales** (`4d6237d84`, `73b03a394`, `f11be256a`, `cb0a04c8b`; docs 51, 54). Un temporal owned (llamada
  con retorno owned o `new` de clase) descartado o receptor tiene dueño oculto. Si su préstamo termina en un valor
  simple, en un owner nuevo o en una escritura de campo, vive hasta el final de la expresión completa y se destruye
  una vez, también cuando una rama de `?:`/`??` no lo creó. El préstamo sigue por campos, getters, resultados no
  owned de llamadas, `?:` y `??`; guardarlo, devolverlo o entregarlo con `yield` es error. Un temporal owned
  entregado a un parámetro que no toma ownership también presta al resultado no owned de la llamada y vive hasta
  el final de la expresión completa (elemento 135a, doc 58).
- **Fuentes de vida conocida** (`1df8e86e3`; docs 55, 56; `docs/3.0/language/known-life-sources.md`). Un solo
  clasificador reconoce temporales owned, locales `raii`, objetos `scope`, excepciones atrapadas, préstamos de
  excepción y locales `pin`; cada comprobación conserva su diagnóstico. Garantía: sin fugas, doble destrucción ni
  escapes cuando la fuente y su vida son conocidas estáticamente; las referencias no owned de parámetros o campos
  son responsabilidad del programador.
- **C-12** (`0b6da967f`, `01f408b04`, `b1f1a9699`; doc 15). Los arreglos dueños destruyen sus elementos vivos antes de
  liberar el bloque.
- **DEC-06** (`01e37532e`; doc 12). `delete[]` de un local `owned T[]` consume el ownership y destruye lo recibido.
- **F1 / F2** (`c86fbb8cd`, `b45a64498`; docs 45, 46). `delete p` de un parámetro `owned` lo consume. Un `T[]` local sin
  `owned` no posee sus referencias: `delete[]` libera sólo el bloque.
- **118** (`1a4cb8062`; doc 44). Un argumento a un parámetro `owned` tiene dueño oculto hasta la llamada.
- **C-13 / P-1 / DEC-14** (`8d46bf5cc`, `73b03a394`, `b8585f145`; docs 33, 53). Toda llamada que declara `throws`, todo
  `throw`, todo `throw;` y todo `await` con error que escapan se atrapan o se declaran, en las ediciones 2 y 3.
- **DEC-10 / P10** (`93bda0690`, `c8b8f42bc`, `2a65e3c59`; docs 13, 30, 31). La excepción atrapada es un préstamo del
  `catch`; sólo `move(e)` la saca. Un parámetro que la recibe no escapa y la variable del `catch` no se captura.
- **C-14 / DEC-08** (`717c9e5f9`, `ac5403f0e`; doc 37). Los futures son move-only; `await` consume el future; uno
  incompleto libera su frame. Un campo `Future<T>` de clase se consume con `await`.
- **C-16** (`0f182413c`). Los generadores destruyen exactamente los locales inicializados y corren el cleanup activo en
  un unwind.
- **C-17 / DEC-15** (`bc8fca456`, `bf4834cf3`, `f648ba11e`; docs 03, 53). El error de un generador async llega completo
  a `MoveNextAsync`; la propagación se prueba sin depender de WER.
- **DEC-12 / P1–P3** (`62ad32baa`, `a3e8ad5fd`, `737503602`, `b0a03bd98`..`e26ddb4bd`, `5cc4e0f9f`; docs 16–28, 35).
  `where T : copy` en miembros, constructores y accesores. Delegados `In*` y reubicación por intercambio. Acceso por
  referencia sin copiar valores move-only.
- **P4** (`80b186758`; doc 29). Un `Span` derivado de un arreglo conserva su mapa de vida.
- **P8 / D-12** (`605114b3b`, `4da911c53`; doc 34). Object ABI 3; una biblioteca con el layout anterior pide recompilar.
  NSMETA no se usa como época (`403b06641` revertido en `4da911c53`).
- **P7** (`c7908a7a0`; doc 36). Restauración por bloques de los archivos históricos (34 archivos restaurados; los
  bloques C++ quedan por contrato).
- **114** (`84cbfefbc`; doc 38). Las acciones de volcado y verificación LLVM bajan con el modelo de miembros, como la
  emisión.
- **F3** (`c306d4e58`; doc 47). En un genérico cerrado, `move(p)` de un parámetro `T` clase sin `owned` copia la
  referencia.
- **F4 (130a)** (`64d826002`; doc 49). Los constructores de un genérico cerrado se analizan y bajan sólo si una
  construcción los elige.
- **139** (doc 59). Excepción a 130a: una biblioteca analiza y baja los constructores públicos y protegidos de toda
  especialización cerrada que publica, porque NSMETA les exige identidad física. Uno inválido se rechaza en el
  productor.
- **I1 (130b)** (`6afc21151`; doc 48). Un argumento `extern C` por valor debe ser ABI-trivial.
- **DiagnosticId** (`88982f3ab`; doc 01). Los IDs históricos conservan su valor; los nuevos se agregan al final.
- **D-10 / DEC-01** (`4bf4a7a18`; doc 07). `nsc` no escribe `error*.log` salvo con `--error-log`.
- **D-09 / DEC-13** (`88c1246c4`; doc 08). La validación de imports contra las DLL del host es opcional y está apagada
  por defecto.
- **Artefactos** (`837325b27`; doc 06). Publicar un artefacto cuyo destino no tiene directorio (`-o nombre.exe`).

## 3. Defectos de la auditoría

- **D-01 reanudación que repite la sentencia**: Corregido (contrato D-01 de arriba).
- **D-02 excepción atrapada destruida y leída**: Corregido por DEC-10 y P10.
- **D-03 slots movidos o borrados destruidos otra vez**: Corregido por DEC-11 y P4.
- **D-04 falsos positivos de `await` síncrono**: Corregido (`8c1559649`).
- **D-05 API genérica inutilizable con structs move-only**: Resuelto por DEC-12 (P1–P3).
- **D-06 manifiesto de superficie falso**: Corregido por DEC-07/08.
- **D-07 cambios de conducta del `await` síncrono**: Decididos: un poll y pánico (DEC-04); propagación de C-17
  conservada (DEC-15).
- **D-08 costo y vida sin medición**: Medido (doc 50); las fugas reproducidas se corrigieron en 131a, 131b, 135 y
  135a.
- **D-09 compilación dependiente del host**: Corregido por DEC-13 y verificado (elemento 119).
- **D-10 `error*.log` en el directorio de trabajo**: Corregido (`4bf4a7a18`).
- **D-11 modo archivo instalado**: **NO VERIFICADO.**
- **D-12 bibliotecas precompiladas con layout viejo**: Rechazadas por Object ABI 3; la `NativeSharpCore` instalada se
  rechaza igual (elemento 120).

Verificaciones sin defecto, elementos 121–129 (sondas en `-O0` y `-O2`):

- `Dictionary`, `HashSet`, `SortedSet`, `SortedList`, `PriorityQueue` y `SortedDictionary` con valores
  destructibles.
- Punteros a structs con destructor y `pin raii T[]`.
- Retornos de `extern C`.

Los defectos que aparecieron ahí se corrigieron como F1–F4 e I1. F5 (operaciones de conjuntos con elemento
clase) queda como rechazo de C-01 por decisión.

## 4. Validación realizada

Es validación **focal**; no certifica la batería.

- Cada elemento tiene sondas propias en `A:\temp\nsc-dec\dNN`, en `-O0` y `-O2`, y su documento en
  `docs/3.0/decisiones-congelamiento/`.
- En cada commit de código se compilaron `nsc` Release y `nsc_component_tests` Debug y Release.
  `Freeze30ContractTests` se compila (`A:\temp\nsc-dec\bt`) pero no se ejecutó hasta el elemento 139.
- En el elemento 139 se ejecutó en `bt`: pasan 1079 de 1085 comprobaciones. Las 6 que fallan son del entorno de
  `bt` (modo archivo sin `lib/` encima del compilador; P3SortedDictionaryGet sin referencia a las colecciones) y
  fallan igual en la base `15dc778d5` (doc 59).
- Desde el elemento 121, `seq.cmd` en cada commit de código contra la referencia `f121`:
  - `all.cmd`: 101 líneas idénticas;
  - `asynccases` 86, `gencases` 76 y `loopcases` 10 PASS;
  - los 66 programas de Freeze30 por línea de comandos en `-O0` y `-O2`;
  - los 27 proyectos de `tests/cases` verifican;
  - `d28` y las 3 bibliotecas.

  `f121` es un punto de la propia campaña, no la base previa.
- En el elemento 136 (sin cambio de conducta), las sondas `d41` dieron el mismo resultado antes y después del
  cambio, y `seq.cmd` fue idéntico a `f135`.

## 5. No verificado: gates finales pendientes

1. Suite completa de NSC Builder 3, una sola vez al final y ejecutada por Alberto.
2. Unitarias, incluida `Freeze30ContractTests`, ejecutadas por Alberto.
3. Árbol histórico (349 proyectos y 52 casos) contra el compilador de referencia.
4. D-11, modo archivo instalado.
5. Instalación y despliegue.
6. Reconciliación de `NSC-DECISIONES-VIGENTES-CONGELAMIENTO-3.0.md` con este documento.

## 6. Limitaciones conocidas

1. Desde el código fuente un future no puede quedar suspendido (no hay poll parcial); `F$drop` de un future ya
   iniciado no tiene frame propio que liberar. Viene de C-14; no se volvió a verificar en esta revisión.
2. Una excepción que sale de un `Main` que la declara termina el proceso con `0xE04E5301`. El reporte de errores
   de Windows demora esa ruta; TL-LANG-376 prueba la propagación sin pasar por ella (DEC-15).
3. F5: `UnionWith`, `SymmetricExceptWith` y `GetViewBetween` con elemento clase siguen rechazados por C-01.
4. Una clave struct move-only no puede usar el constructor con `HashFunc` de `Dictionary`/`HashSet`
   (`where TKey : copy`, P2).
5. El diagnóstico de un constructor genérico elegido inválido sale en su cuerpo, no en el `new` (doc 49).
6. Límites del contrato de vida (`docs/3.0/language/known-life-sources.md`, medidos en doc 56):
   - las referencias no owned de parámetros o campos no se comprueban;
   - un préstamo derivado (campo, getter, llamada, `?:`) de un local `raii` o `scope`, o de una excepción
     atrapada, compila y puede sobrevivir a su fuente;
   - el análisis de llamadas es conservador: puede tratar como préstamo un resultado que no lo es.
7. Rendimiento: las corrutinas que nunca suspenden reservan frame en el heap en cada llamada (doc 50). Queda
   registrado para la fase de rendimiento, sin cambiar ahora el ABI de frames.

## 7. Historia

La versión anterior de este documento, con su resumen, hallazgos, limitaciones y los anexos A y B, está en
`9b224f4df` y `aa80795b4`. El plan original de 108 elementos y su reconciliación están en el proyecto NSC-IDE
(`NSC-PLAN-CONGELAMIENTO-3.0.md`).
