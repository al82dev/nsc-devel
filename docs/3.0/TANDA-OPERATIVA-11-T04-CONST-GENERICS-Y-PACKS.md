# Tanda operativa 11 — T04 const generics y type packs

## Objetivo

Llevar const generics y packs de tipos desde los helpers aislados de T04 hasta el compilador
productivo Windows x64. La entrega debía conservar identidad canónica, afectar layout real, llegar
a LLVM/PE O0, cruzar una frontera `.nsmeta`, terminar bajo límites explícitos y fallar de forma
causal ante usos inválidos.

El alcance efectivo no declara terminado todo T04. Los aliases transparentes 2.x continúan
productivos; opaque aliases y especialización controlada permanecen diseñados pero todavía no
forman parte del frontend productivo. Esta separación evita publicar sintaxis o ABI sin cerrar la
frontera de representación y coherence.

## Situación encontrada

- `GenericSemantics` ya describía valores const, packs, identidad de alias y dominancia de
  especialización, pero sólo como contrato unitario.
- El AST guardaba todo parámetro genérico como un nombre de tipo; no podía distinguir `T`,
  `const int N` ni `pack Ts`.
- El parser sólo admitía tipos dentro de `<...>` y exigía por lo menos un argumento.
- El materializador Stage 44 esperaba aridad fija y sólo sustituía parámetros de tipo uno a uno.
- No existía una ruta para sustituir `N` dentro de expresiones/dimensiones, contar un pack ni
  reenviarlo a otro tipo genérico.
- `.nsmeta` ya transportaba el source template público; faltaba demostrar que la nueva sintaxis se
  rehidrataba con la misma identidad y semántica.

## Diseño implementado

### Descriptores de parámetros

Cada declaración conserva ahora, en paralelo con el nombre compatible 2.x, un descriptor con:

- parámetro de tipo ordinario;
- valor const con su tipo declarado;
- pack de tipos;
- pack de valores reservado, no materializable todavía.

La sintaxis edition 3 aceptada es `const <builtin> N` y `pack Ts`. Un pack debe ser el último
parámetro. Los tipos const admitidos en esta vertical son `bool`, `char` y los enteros builtin. Un
`where` sobre const/pack se rechaza en la declaración, no lejos de ella durante una instancia.

### Argumentos const e identidad

`<4>`, `<4u>`, `<true>` y `<false>` se parsean como valores de compile time, no como tipos. El
materializador valida la clase del valor y el rango del tipo declarado, y después lo normaliza a
la identidad de la declaración. Por ello `Buffer<int, 4>` y `Buffer<int, 4u>` para `const int N`
comparten exactamente una especialización.

La clave estable incluye `const:builtin:<tipo>=<valor>`. El nombre físico añade el hash estable ya
usado por Stage 44. No existe diccionario runtime, boxing ni argumento ABI oculto.

Durante clonación, una referencia a `N` se vuelve literal entero/booleano. Esto cubre cuerpos como
`return N` y dimensiones como `T[N]`; el layout resultante se calcula por el pipeline ordinario.

### Packs de tipos

Una plantilla con pack final usa aridad `fija+pack` y admite cero, uno o muchos argumentos. El pack
se conserva como lista durante la clonación y habilita dos operaciones productivas:

- `packcount(Ts)` se reduce a un literal entero en compile time;
- `Ts...` expande el pack dentro de otra lista de argumentos genéricos.

`F<>()` se distingue de `F()` mediante un bit sintáctico en `CallExpression`: el primero es una
especialización explícita de pack vacío; el segundo sigue solicitando inferencia. El bit se limpia
al reescribir la llamada, para que recorridos posteriores no intenten especializarla de nuevo.

Cada expansión acepta como máximo 1024 elementos. El límite se aplica mediante
`validatePackExpansion` y se publica en `--dump-project-generics` junto con los límites de
monomorfización de la tanda 10. Una expansión residual, fuera de una lista genérica o sin pack
declarado, falla cerrada.

### Frontera `.nsmeta`

No se inventó una segunda representación de templates. La metadata pública conserva el source
template edition 3; el consumidor lo lexea y parsea con los mismos descriptores. Un productor
`StaticLibrary` con `Buffer<T,const int N>` y `Types<pack Ts>` fue emitido como `.lib/.nsmeta`; un
consumidor independiente especializó ambos, enlazó en O0 y terminó en 42.

## Cambios por archivo

### Modificados

- `include/nativesharp/AST/AstNode.h`, `src/AST/AstNode.cpp`
  - agregan y preservan `GenericParameterSyntax` y su clase semántica.
- `include/nativesharp/AST/TypeSyntax.h`, `src/AST/TypeSyntax.cpp`
  - conservan la marca explícita de expansión `...`.
- `include/nativesharp/Language/GenericSemantics.h`,
  `src/Language/GenericSemantics.cpp`
  - codifican y decodifican el marcador interno determinista de argumento const.
- `src/Parser/Parser.cpp`
  - parsea parámetros/argumentos const, packs, listas vacías y `Ts...`;
  - reserva estas formas a edition 3;
  - diagnostica packs no finales y constraints inválidos;
  - distingue `F<>()` de inferencia `F()`.
- `include/nativesharp/Frontend/ProjectGenericModel.h`,
  `src/Frontend/ProjectGenericModel.cpp`
  - usa aridad variable final, normaliza valores, sustituye literales, materializa packs,
    implementa `packcount`, forwarding y rechazos residuales;
  - integra el límite de expansión y conserva contexto durante materialización anidada.
- `src/Frontend/ProjectGenericModelDumper.cpp`
  - publica `pack-expansion-limit=1024`.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra c61 para el baseline final que ejecutará Alberto.
- `docs/3.0/TANDA-04-GENERICOS-3.0.md`
  - refleja la vertical productiva y separa con precisión lo que continúa abierto.

### Nuevos

- `tests/cc1/c61_ns3_const_generics_packs/c61_ns3_const_generics_packs.nsproj`
  - fixture mínima edition 3.
- `tests/cc1/c61_ns3_const_generics_packs/Main.ns`
  - testigo O0 de enteros/bool, identidad canónica, layouts 16/32, pack vacío/múltiple,
    forwarding y métodos genéricos; devuelve 42.
- `docs/3.0/TANDA-OPERATIVA-11-T04-CONST-GENERICS-Y-PACKS.md`
  - registro técnico, archivos, evidencia, límites y deuda de esta tanda/commit.

No se creó código fuente fuera de `A:\nsc-devel`. Los proyectos, ejecutables, librerías, metadata y
logs temporales de los probes se eliminaron antes del commit.

## Mejora concreta

- Estructuras como `Buffer<T,const int N>` obtienen layout reproducible sin heap ni estado runtime.
- Valores equivalentes no duplican código por el sufijo léxico del literal.
- Packs vacíos y no vacíos tienen claves distintas y deterministas.
- El forwarding reutiliza el materializador existente y su cache en lugar de generar AST paralelo.
- Templates edition 3 conservan comportamiento al cruzar `.nsmeta`.
- Errores de tipo, overflow, edition, posición de pack y expansión inválida se localizan en fuente.
- Una entrada hostil no puede expandir un pack sin límite visible.

## Validación dirigida ejecutada

1. Construcción sólo de `nsc`, configuración Debug/O0, target `nsc`: correcta.
2. `c61_ns3_const_generics_packs` con `--emit-project-oop-exe -O0`: correcta; exit 42.
3. Layout dirigido:
   - `Buffer<int,4>`: size 16, align 4, campo `int[4]`;
   - `Buffer<int,8>`: size 32, align 4;
   - `Buffer<int,4>` y `Buffer<int,4u>`: una especialización y cache hit.
4. Dos dumps genéricos consecutivos: idénticos; límites 64/16384/1024 publicados.
5. Productor `StaticLibrary` `.lib/.nsmeta` y consumidor separado O0: emisión, rehidratación,
   enlace y ejecución correctos; exit 42.
6. Siete probes negativos individuales, todos con exit distinto de cero y fragmento causal:
   tipo const incorrecto, overflow, edition 2, pack no final, valor dentro de type pack,
   expansión sin pack y `where` sobre pack.
7. El target unitario T04 no está generado en esta configuración `build-clang`; el intento por
   nombre respondió `unknown target`. No se reconfiguró para habilitar suites y no se invocó CTest.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota.

## Límites y deuda explícita

- Const generics productivos: sólo literales `bool`/enteros no negativos y tipos builtin. Enums,
  structural values y evaluación checked de expresiones constantes continúan abiertos.
- Packs productivos: type pack final, `packcount` y forwarding. Indexado, folds, zipped packs y
  value packs continúan abiertos.
- La inferencia no deduce valores const ni la longitud de un pack; se escriben explícitamente.
- Opaque aliases y especialización controlada siguen siendo contratos aislados. No se publica una
  sintaxis parcial porque aún faltan privacidad de representación/metadata para el primero y
  coherence/orden parcial cross-module para la segunda.
- Lambdas, tuples/method groups y propagación contextual profunda permanecen como deuda F04.

La tanda operativa 11 queda cerrada con una vertical real F02/F03; T04 maestro permanece
`IMPLEMENTING` hasta cerrar esas capacidades y los gates completos de ABI, tooling y rendimiento.
