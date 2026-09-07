# NativeSharp: historia de una implementación, paso a paso

## Estado de esta carpeta

Esta carpeta contiene, por ahora, el **plan maestro de la obra**. No pretende hacer pasar un índice
por el libro terminado. Su propósito es fijar, antes de redactar cientos de capítulos, qué debe
contarse, con qué grado de detalle, en qué orden, con qué fuentes y qué pruebas deberá poder repetir
el lector.

La obra terminada tendrá una exigencia poco habitual: si desapareciera el código fuente actual y
sólo sobreviviera esta carpeta, un equipo competente debería poder reconstruir `nsc` desde un
`main` vacío hasta NativeSharp 1.0 sin tener que adivinar las decisiones esenciales.

Eso obliga a documentar cuatro historias a la vez:

1. **La historia humana:** por qué nació NativeSharp, qué se quería conservar de C# y qué se quería
   evitar de CLR, GC, reflection y toolchains pesados.
2. **La historia del lenguaje:** cómo se congelaron sintaxis, tipos, memoria, ownership, ABI,
   objetos, genéricos, excepciones e interoperabilidad.
3. **La historia del compilador:** cómo apareció cada archivo, clase, algoritmo, verificador, prueba
   y producto binario.
4. **La historia de la evidencia:** qué se creyó terminado, qué estaba realmente en el árbol, qué
   regresó, qué hubo que recuperar y qué certificado permite afirmar que una etapa sí quedó cerrada.

## Lo que significa «paso a paso»

No significa una lista como ésta:

```text
1. escribir lexer
2. escribir parser
3. generar LLVM
```

Significa poder seguir transiciones pequeñas y comprobables:

```text
main.cpp vacío
  ↓
ejecutable que retorna 0
  ↓
argumentos de línea de comandos
  ↓
lectura de un archivo sin interpretar su contenido
  ↓
FileId estable
  ↓
SourceLocation como offset
  ↓
índice de comienzos de línea
  ↓
primer diagnóstico con archivo, línea y columna
  ↓
primer TokenKind
  ↓
primer carácter consumido por el lexer
  ↓
primer token EOF
  ↓
primer identificador
  ↓
...
```

Cada transición futura deberá decir qué problema obliga a crearla, cuál es el cambio mínimo, qué
archivo se añade o modifica, cómo compilar, qué salida exacta esperar, qué prueba falla antes, qué
prueba pasa después y qué limitación sigue deliberadamente abierta.

## Documentos de planificación

1. [`00_PROPOSITO_ALCANCE_Y_PROMESA.md`](00_PROPOSITO_ALCANCE_Y_PROMESA.md) define la promesa de la
   obra, el público, el nivel de profundidad y lo que queda fuera.
2. [`01_FUENTES_EVIDENCIA_Y_VERDAD_HISTORICA.md`](01_FUENTES_EVIDENCIA_Y_VERDAD_HISTORICA.md)
   establece cómo se investigarán chats, Markdown, ZIP, código, pruebas, logs y Git sin convertir
   una afirmación antigua en un hecho por accidente.
3. [`02_MAPA_HISTORICO_Y_RELOJES_DE_VERSION.md`](02_MAPA_HISTORICO_Y_RELOJES_DE_VERSION.md) separa
   especificaciones, stages pedagógicos, versiones de producto, versiones de ABI y revisiones de
   NSMETA.
4. [`03_INDICE_MAESTRO_HASTA_1_0.md`](03_INDICE_MAESTRO_HASTA_1_0.md) es el contenido editorial
   detallado, desde el origen y el primer `main` hasta el contrato público 1.0.
5. [`04_CONTRATO_DE_REDACCION_DE_CADA_CAPITULO.md`](04_CONTRATO_DE_REDACCION_DE_CADA_CAPITULO.md)
   fija la anatomía obligatoria de cada capítulo y evita capítulos de cien palabras disfrazados de
   documentación.
6. [`05_PROTOCOLO_DE_RECONSTRUCCION_Y_SNAPSHOTS.md`](05_PROTOCOLO_DE_RECONSTRUCCION_Y_SNAPSHOTS.md)
   define cómo los lectores reconstruirán cada checkpoint y cómo se conservarán deltas, fixtures,
   manifests y resultados.
7. [`06_PLAN_DE_INVESTIGACION_DE_CHATS_Y_MARKDOWNS.md`](06_PLAN_DE_INVESTIGACION_DE_CHATS_Y_MARKDOWNS.md)
   enumera el trabajo de recuperación histórica que todavía debe hacerse antes de afirmar que la
   narración está completa.
8. [`07_ORDEN_DE_PRODUCCION_Y_DEFINITION_OF_DONE.md`](07_ORDEN_DE_PRODUCCION_Y_DEFINITION_OF_DONE.md)
   establece el orden de escritura y los gates para declarar terminada la obra hasta 1.0.
9. [`PLANTILLA_CAPITULO.md`](PLANTILLA_CAPITULO.md) será copiada para cada capítulo real.

## Estado histórico que ya puede afirmarse

La investigación inicial cruzó el árbol actual, los documentos conservados y conversaciones
históricas accesibles. El mapa general confirmado es:

```text
00–05  contratos del lenguaje, gramática, tipos, memoria y ABI
06–20  compilador procedural completo hasta LLVM, COFF, runtime y PE
21–30  proyectos, namespaces, clases, construcción, layout y codegen OOP inicial
31–39  receivers, herencia, virtual dispatch, destrucción, miembros ricos y RTTI
40–45  value types, arrays/string, delegates, closures, genéricos AOT y EH Win64
46–50  productos de biblioteca, C ABI, SDK/NSMETA, RAII, owned y borrowed
51–57  Core, colecciones, sistema, interop y correctness del lenguaje
0.7.x  gates, observabilidad y cierre de defectos reportados por consumidores
0.8.0  cierre del toolchain y workarounds que bloqueaban programas reales
0.9.0  bibliotecas precompiladas, .lib/DLL y NSMETA 1.5
1.0-E1..E4  objetos/genéricos/lifetimes cross-module y congelación ABI/NSMETA 1.8
1.0 RC  estabilización, corpus externo, diagnósticos de compatibilidad e instalación limpia
1.0.0  contrato público, todavía no proclamado por el árbol estudiado
```

El producto observado durante esta planificación todavía se identifica como:

```text
nsc 0.9.0+build.20260907T172233Z.312a1b69a3ec28c2
NativeSharp 0.1, stage 57
```

Aunque ya existe **Native ABI 1.0**, **Runtime ABI 1.0** y **NSMETA 1.8**, esos números no autorizan
a llamar `1.0.0` al producto. La diferencia será explicada y preservada en toda la obra.

## Reglas editoriales innegociables

- No se resumirá una etapa sólo porque exista un buen documento previo; el documento previo será
  una fuente y se reconstruirá el razonamiento.
- No se afirmará que una feature existió en un stage sólo porque un chat la declaró terminada.
- No se usará Git como genealogía del periodo anterior a su uso sistemático.
- No se borrarán errores, regresiones, intentos fallidos ni recuperaciones de la narración.
- No se mostrarán bloques enormes de código sin explicar cada responsabilidad y cada invariante.
- No habrá saltos mágicos entre AST, HIR, MIR, LLVM, COFF y PE.
- No bastará con “todos los tests pasan”: se explicará qué demuestra cada familia de pruebas y qué
  no puede demostrar.
- Cada checkpoint reconstruible tendrá un programa positivo, un programa negativo, comandos de
  construcción, salida esperada y un inventario del árbol.
- Las decisiones posteriores se explicarán cuando aparezca el problema que las hace necesarias;
  no se contaminará el primer lexer con conocimientos que sólo pertenecen a Stage 50.
- Todo hecho incierto quedará marcado como incierto, con una tarea concreta para resolverlo.

## Resultado esperado de la siguiente fase

Después de aprobar este plan, la fase de escritura comenzará por la recuperación de fuentes
tempranas y la creación de checkpoints reconstruibles. Los capítulos no se redactarán como una
enciclopedia retrospectiva que empieza desde el compilador terminado, sino como una construcción
acumulativa donde el lector sólo puede usar las piezas que ya se introdujeron.

