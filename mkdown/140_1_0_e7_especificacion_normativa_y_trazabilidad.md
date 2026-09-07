# 140 — 1.0-E7: especificación normativa y trazabilidad

**Fecha:** 2026-09-07  
**Estado:** implementación cerrada localmente  
**Snapshot:** Stage 60  
**Certificación:** pendiente de instalación vigente y G00

## Resultado

E7 convirtió la semántica existente de NativeSharp en un contrato público auditable. Antes de esta
entrega, las decisiones estaban implementadas y ampliamente probadas, pero había que reconstruirlas
leyendo el libro, documentos históricos, suites y comentarios del compilador. Eso permitía saber
qué hacía el árbol, pero no contestaba de forma única qué estaba obligado a hacer 1.0.

Ahora existen tres piezas que viajan juntas:

1. `spec/NativeSharp_Language_Specification_1_0.md`, autoridad normativa candidata;
2. `spec/NativeSharp_Grammar_1_0.ebnf`, superficie sintáctica contractual;
3. `spec/NativeSharp_1_0_conformance.map`, trazabilidad entre cláusula, implementación y evidencia.

La especificación no cambia la versión pública del lenguaje antes de tiempo. El compilador sigue
identificándolo como NativeSharp 0.1; el nombre 1.0 describe el contrato que se está preparando.

## Qué quedó especificado

### Texto y validez

- tokens, comentarios, identificadores y literales;
- gramática de unidades, declaraciones, miembros, statements y expresiones;
- precedencia y asociación;
- proyectos, namespaces y clases parciales;
- resolución de nombres, accesibilidad, conversiones y selección de overload;
- obligación de rechazar código inválido sin producir un artefacto utilizable.

### Sistema de tipos y ejecución

- categorías de clase, struct, interface, enum y delegate;
- campos, métodos, propiedades, indexadores, operadores, constructores y destructores;
- herencia, interfaces y dispatch;
- arrays dinámicos, fixed arrays y sus dos niveles de ownership;
- paso por valor y modos `ref`, `out` e `in`;
- punteros, function pointers y reinterpretación explícita;
- delegates, closures y eventos.

### Semántica física

- tamaños primitivos y target `x86_64-pc-windows-msvc`;
- wrap entero, casts truncantes, división, residuo y shifts;
- punto flotante IEEE 754 sin banderas rápidas;
- orden de evaluación, cortocircuito e inicializadores de arrays;
- definite assignment e inicialización física a cero;
- inicialización estática concurrente y reentrante;
- ownership, `raii`, préstamos, `move` y cleanup;
- excepciones nativas tipadas y frontera de callbacks C;
- accesos ordinarios, atomics secuencialmente consistentes y publicación por sincronización;
- layout de agregados, clases y delegates;
- calling conventions y contratos binarios versionados;
- monomorfización y conservación semántica entre O0 y O2.

## Una decisión central: separar regla, código y evidencia

Una regla escrita sólo en una prueba obliga a inferir su significado por ejemplos. Una regla
escrita sólo en código obliga a confundir la implementación actual con el contrato. Una regla
escrita sólo en prosa puede quedar obsoleta sin poner rojo el build.

E7 asigna un identificador estable a cada decisión, por ejemplo:

```text
NS-NUM-003    división entera y traps
NS-OWN-002    movimiento y consumo del origen
NS-MEM-002    ordenamiento de atomics
NS-LAYOUT-001 layout de agregados
NS-ABI-002    versionado binario
```

Cada entrada del mapa debe declarar:

- estado `implemented` o `excluded`;
- título humano;
- uno o más archivos que implementan la regla;
- uno o más archivos de evidencia ejecutable.

`excluded` no significa deuda escondida. Se usa cuando una forma es deliberadamente inválida y el
frontend contiene tanto el rechazo como su prueba. El ejemplo actual es el almacenamiento estático
`owned`, que no puede prometer destrucción automática mientras no exista finalización de módulo.

## Stage 60: el contrato también puede romper el build

`tests/unit/Stage60LanguageContractTests.cpp` valida mecánicamente:

- existencia y lectura de especificación, gramática y mapa;
- identificadores `NS-*` únicos;
- correspondencia exacta en ambos sentidos entre headings y entradas;
- disposición contractual válida;
- título no vacío;
- al menos un archivo de implementación y uno de evidencia;
- rutas relativas, portables y existentes;
- presencia de las producciones raíz de la gramática;
- presencia de ownership en la gramática;
- target exacto;
- instalación de `spec/` como parte del producto;
- Stage 60 como identidad del snapshot.

Con las 42 cláusulas actuales, la suite ejecuta **954 checks**. Ese número no mide la corrección
semántica de las 42 reglas; mide que ninguna quedó huérfana. Las suites focales citadas por el mapa
continúan siendo quienes prueban el comportamiento.

## Archivos nuevos

### `spec/NativeSharp_Language_Specification_1_0.md`

Contrato normativo candidato. Reúne decisiones antes dispersas y establece su autoridad frente al
libro y la historia. No borra los documentos antiguos: éstos siguen explicando cómo y por qué se
llegó al contrato.

### `spec/NativeSharp_Grammar_1_0.ebnf`

Gramática legible por personas y herramientas. Congela la superficie reconocible y la precedencia
de expresiones. Una extensión del parser que no actualice la gramática no forma parte de 1.0.

### `spec/NativeSharp_1_0_conformance.map`

Índice plano y revisable de las 42 cláusulas. Sus rutas usan `/` y son relativas al repositorio,
por lo que no dependen de la máquina que ejecute CTest.

### `tests/unit/Stage60LanguageContractTests.cpp`

Auditor del contrato. No enlaza el frontend porque no vuelve a probar semántica: verifica la red de
trazabilidad y deja a cada suite especializada como evidencia de su cláusula.

## Archivos modificados

### `CMakeLists.txt`

- presenta el snapshot como Stage 60;
- registra `nsc.stage60.language-contract`;
- entrega al test la raíz del repositorio;
- instala todo `spec/` en `share/nativesharp/spec`.

### `include/nativesharp/Basic/Version.h`

`kCompilerStage` pasa de 57 a 60. La versión compatible del compilador permanece en 0.9.0 y la del
lenguaje en 0.1; E7 no finge una promoción comercial.

### `tests/unit/Cc1BuildIdentityTests.cpp`

Actualiza el canario que obliga a que la identidad visible coincida con el snapshot real.

### `mkdown/121_next_steps_for_the_1_0_glory.md`

Registra E7 como cierre local posterior al baseline certificado E1–E6.

### `mkdown/CONTENIDO.txt`

Incorpora este documento a la secuencia didáctica permanente.

## Evidencia focal

El compilador de desarrollo se reconstruyó correctamente:

```text
nsc 0.9.0+build.20260907T194804Z.227dde767aa70c8a
NativeSharp 0.1, stage 60
SHA-256 F65A4E45403E4F04BF49D5397DC7EB408E7BF9E49834E100A50991AEC92BC110
```

La suite propia de E7 terminó:

```text
lld-link tree  PASS: 954 checks
NSLink tree    PASS: 954 checks
```

La selección focal de cláusulas ejecutó 37 pruebas por árbol:

```text
lld-link tree  37/37 PASS
NSLink tree    37/37 PASS
```

Incluyó léxico, parser, nombres, conversiones, enteros, inicialización estática, excepciones,
ownership, definite assignment, atomics, control de flujo, layout e identidad del build.

## La corrida paralela inválida

La primera validación lanzó los dos árboles al mismo tiempo. Varias suites históricas crean rutas
temporales con nombres fijos y ambas configuraciones usan además el mismo compilador instalado.
Las dos corridas se pisaron archivos `.rsp`, logs y ejecutables. El síntoma fue concluyente:
pruebas deterministas fallaron en casos diferentes con “file is being used by another process”.

Ese resultado no se contó como evidencia negativa de E7. Tampoco se ocultó. Se clasificó como
corrida inválida por interferencia del arnés y se repitió correctamente:

1. las 37 pruebas con el árbol lld-link;
2. sólo después de terminar, las mismas 37 con el árbol NSLink.

Ambas repeticiones pasaron completas. La lección queda incorporada al método: estos dos árboles se
pueden construir en paralelo, pero sus suites que usan temporales compartidos deben ejecutarse en
serie hasta que el arnés les asigne raíces distintas.

## Corrección encontrada por la matriz completa

La primera matriz completa posterior a E7 encontró un fallo real del arnés:

```text
297/298 PASS
nsc.version FAIL: la expresión regular todavía exigía `stage 57`
salida real: NativeSharp 0.1, stage 60
```

No era un error de generación de código. `nsc --version` publicaba correctamente Stage 60 y
`nsc.version.identity` también lo verificaba. El problema era una segunda copia literal del número
de stage dentro de `CMakeLists.txt`.

La corrección elimina la duplicación sin debilitar el test:

- CMake extrae `kCompilerStage` directamente de `Version.h` durante la configuración;
- `nsc.version` exige ese stage exacto al compilador instalado, además de comprobar la forma de la
  versión y el identificador de construcción;
- `nsc.version.identity` sigue comprobando el número exacto contra `kCompilerStage`.

Así, avanzar el stage exige actualizar una sola fuente de verdad y la prueba continúa detectando una
instalación atrasada. Después de la corrección, `nsc.version` y `nsc.version.identity` se revalidaron
en los dos árboles.

## Instalación aislada

La instalación se probó bajo `_targeted`, sin reemplazar el compilador certificado del sistema. El
producto aislado contiene:

```text
bin/nsc.exe
lib/...
share/nativesharp/spec/NativeSharp_Language_Specification_1_0.md
share/nativesharp/spec/NativeSharp_Grammar_1_0.ebnf
share/nativesharp/spec/NativeSharp_1_0_conformance.map
```

Esto demuestra que la especificación forma parte del producto preparado para instalar, no sólo del
repositorio de desarrollo.

## Estado de cierre

E7 queda cerrado localmente. Para promoverlo a baseline certificado aún falta el flujo externo:

1. reconstruir e instalar el producto vigente;
2. ejecutar G00 con árbol inmóvil;
3. confirmar `S0 == S1`;
4. confirmar `built_nsc == installed_nsc`;
5. registrar run, hashes, etapas y casos en un acta nueva.

Hasta entonces, el último baseline certificado continúa siendo el documento 139. La existencia de
un binario Stage 60 local no reescribe esa historia.
