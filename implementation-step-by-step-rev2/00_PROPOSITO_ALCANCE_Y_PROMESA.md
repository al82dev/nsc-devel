# 00 — Propósito, alcance y promesa de la obra

## 1. La pregunta que debe responder

La obra completa debe responder con evidencia a esta pregunta:

> ¿Cómo se puede construir NativeSharp desde una carpeta vacía, entendiendo por qué existe cada
> pieza, hasta obtener el compilador y el contrato público de la versión 1.0?

“Construir” tiene aquí un significado fuerte. No basta con reproducir la forma final de los
archivos. Un clon escrito a partir de una fotografía del árbol podría compilar y, aun así, no
enseñar nada sobre por qué existen HIR y MIR separados, por qué un borrow no genera cleanup, por
qué el right operand de un shift no participa como el izquierdo en el tipo del resultado o por qué
una DLL necesita metadata semántica además de exports físicos.

El lector deberá poder reconstruir tanto el producto como su modelo mental.

## 2. Por qué nació NativeSharp

La introducción histórica se redactará desde las conversaciones y especificaciones originales, no
desde una explicación inventada a posteriori. La hipótesis que ya aparece consistentemente en las
fuentes es ésta:

- conservar una sintaxis clara y familiar para quien conoce C#;
- producir código nativo AOT para Windows x64;
- evitar dependencia conceptual de CLR, IL y reflection dinámica;
- evitar un GC obligatorio y hacer visibles lifetime, ownership y costos;
- permitir interoperabilidad directa con C, C++, Win32, `.lib`, DLL y objetos COFF;
- mantener un frontend, IR y semántica propios, usando LLVM como backend y no como definición del
  lenguaje;
- caminar hacia un toolchain que pueda instalarse y utilizarse de forma reproducible;
- documentar el compilador como objeto educativo, no sólo como producto.

Cada punto se verificará contra la fuente más temprana disponible. Si una ambición apareció más
tarde —por ejemplo, independencia progresiva del linker— se narrará en el momento histórico real y
no como si hubiera formado parte del primer día.

## 3. Públicos de la obra

La documentación servirá simultáneamente a cinco lectores:

### 3.1 El principiante serio

Puede programar, pero nunca construyó un compilador. Necesita que `FileId`, arenas, CFG, dominance,
SSA, ABI, COFF y unwind se introduzcan desde el problema concreto que resuelven.

### 3.2 El estudiante universitario

Necesita algoritmos, invariantes, ejercicios, pruebas negativas, complejidad, referencias entre
representaciones y oportunidad de implementar una pieza antes de ver la solución.

### 3.3 El ingeniero de compiladores

Necesita precisión: estructuras, ownership de objetos C++, contratos de API, orden de passes,
algoritmos exactos, formatos binarios, fallos observados y justificación de desviaciones frente a
implementaciones de libro.

### 3.4 El mantenedor futuro de NativeSharp

Necesita saber cuál archivo es autoridad, qué deuda fue deliberada, qué gate protege un contrato,
qué números de versión significan cosas distintas y dónde una “simplificación temporal” se volvió
permanente.

### 3.5 El equipo que sólo conserva esta carpeta

Es el lector de desastre. Debe poder crear el proyecto, introducir los archivos en el orden
correcto, ejecutar los tests y llegar a snapshots equivalentes aunque el repositorio original haya
desaparecido.

## 4. Alcance temporal

El final de esta obra es la **versión de producto NativeSharp 1.0.0**. El plan incluye:

- antecedentes, motivación y contratos 00–05;
- stages pedagógicos 06–57;
- rondas de correctness y maduración posteriores;
- versiones 0.7.x, 0.8.0 y 0.9.0;
- megaentregas 1.0-E1 a E4 ya observadas;
- estabilización, candidatos de release y gate final necesarios para publicar 1.0.0.

No termina en el estado actual sólo porque el ABI ya se llame 1.0. Termina cuando la identidad de
producto, los artefactos publicados y la evidencia de instalación corresponden a 1.0.0.

## 5. Alcance técnico

La reconstrucción cubrirá al menos:

- proyecto C++20, construcción y driver;
- SourceManager y diagnostics;
- lexer, parser, syntax AST y recuperación de errores;
- símbolos, sistema de tipos, conversiones y semantic model;
- HIR, MIR, CFG, dominance y SSA;
- pass manager y optimizaciones propias;
- lowering y optimización LLVM;
- Windows x64 ABI, COFF, PE, runtime y linking;
- proyectos, referencias, namespaces, aliases y lookup;
- clases, structs, layout, herencia, virtual dispatch, interfaces y RTTI;
- properties, indexers, operators y conversiones definidas por usuario;
- arrays, string, delegates, events, lambdas y closures;
- genéricos AOT por monomorfización;
- excepciones nativas, cleanup, `finally` y `using`;
- `.lib`, DLL, C ABI, headers, `.nsmeta` y consumo cross-module;
- RAII, move, `owned`, `borrowed`, escape analysis y control de flujo;
- biblioteca NativeSharp.Core, colecciones, filesystem, entorno, threads y diagnósticos;
- correctness numérico, layouts, packing, unions, function pointers, `ref/out/in`, `switch`,
  `stackalloc`, definite assignment y raíz de tipos;
- gates reproducibles, doble linker, manifests e identidad de binarios;
- compatibilidad de bibliotecas precompiladas y congelación ABI.

## 6. Lo que no se prometerá

La historia hasta 1.0 no se convertirá artificialmente en la historia de otro producto. Salvo que
las fuentes demuestren lo contrario, no prometerá:

- CLR, IL/MSIL o JIT obligatorio;
- reflection dinámica compatible con .NET;
- GC obligatorio;
- compatibilidad binaria con assemblies .NET;
- todos los rincones de C#;
- Linux, macOS o otra plataforma como requisito de 1.0;
- un IDE completo;
- self-hosting si no forma parte del gate real de 1.0;
- reemplazo total de LLVM si eso pertenece a una hoja de ruta posterior;
- documentación de NSASM/NSLink como si fueran el mismo proyecto que `nsc`.

NSLink aparecerá donde afecte la certificación de NativeSharp y su independencia de `lld-link`,
pero su historia exhaustiva deberá vivir en una obra propia.

## 7. Profundidad mínima

No se fijará una cuota absurda de palabras por capítulo. Algunos cambios pequeños requieren menos
texto que la construcción SSA o el EH Win64. Sí se fijan obligaciones de contenido.

Un capítulo técnico no estará completo si no contiene:

- estado exacto de entrada;
- problema observable;
- alternativas consideradas;
- decisión y razón;
- delta de archivos;
- explicación de estructuras y algoritmos;
- recorrido de al menos un ejemplo;
- caso negativo y diagnóstico;
- forma de construir y probar;
- salida esperada;
- limitaciones que permanecen;
- estado exacto de salida;
- fuentes primarias y nivel de confianza.

Cuando el cambio atraviese varias capas, deberá seguirse así:

```text
source
  → tokens
  → syntax AST
  → binding / tipo
  → HIR
  → MIR
  → CFG / SSA si aplica
  → LLVM
  → COFF / PE / metadata si aplica
  → comportamiento observado
```

Omitir una capa porque “es obvia” estará prohibido.

## 8. Orden pedagógico frente a orden del pipeline

La obra tendrá dos recorridos simultáneos:

1. **Orden histórico de implementación**, que explica qué se construyó y cuándo.
2. **Orden físico de una compilación**, que explica por dónde pasa un programa en el compilador ya
   terminado.

No son iguales. Dominance se implementó después del CFG, pero durante una compilación ambos operan
en un orden preciso dentro del middle-end. El índice mantendrá enlaces bidireccionales para que el
lector no confunda “capítulo anterior” con “fase anterior en runtime”.

## 9. La honestidad como requisito técnico

NativeSharp ya ofrece un caso histórico que debe conservarse: Stages 35 y 36 fueron descritos,
pero una auditoría posterior encontró que sus cambios no estaban físicamente en el snapshot usado
para avanzar. La recuperación posterior forma parte de la historia.

Ocultarla produciría un relato más limpio y mucho menos útil. La obra mostrará:

- cómo se detectó la discordancia;
- qué evidencia la demostró;
- por qué un documento “terminado” no bastaba;
- cómo se recuperó el código;
- qué tests impiden repetir el error.

La misma regla se aplicará a fallos de drivers, expectations obsoletas, pruebas que midieron otro
binario, matrices incompletas, problemas de entorno y regresiones reales.

## 10. Promesa final

La documentación terminada no dirá solamente “así quedó NativeSharp”. Dirá:

```text
éste era el problema
éste era el árbol antes
ésta fue la decisión
éste fue el cambio mínimo
esto se rompió
así se diagnosticó
así se corrigió
esto demuestra el test
esto todavía no existe
éste es el snapshot desde el que puedes continuar
```

Ésa es la unidad fundamental de la obra.

