# Tanda operativa 07 — T02-F01/F02/F03: tuplas y deconstrucción

## Objetivo

Convertir la sintaxis/helper T02 existente en una vertical productiva del compilador: tipos tupla,
literales, conversiones elemento por elemento y deconstrucción deben cruzar parser, binding,
semántica, layout, HIR, MIR, LLVM y ABI Windows x64. Esta tanda no declara terminado T02 completo:
records/promoted/spreads pertenecen a la tanda 08 y ownership/tooling conservan dueños posteriores.

## Hallazgo inicial

Antes de esta tanda existían `TupleExpression` y `AggregateSemantics`, pero no había un tipo tupla
en `TypeSyntax`, identidad canónica productiva, binding de nombres, layout compartido, catálogo MIR,
carrier LLVM ni lowering de deconstrucción. El parser podía dibujar `(a, b)`, pero el compilador no
podía declarar, retornar, pasar, convertir ni ejecutar una tupla.

## Cambios funcionales

### Tipo estructural e identidad

- `TypeSyntax` marca tuplas y conserva nombres de elementos por posición.
- Los nombres no participan en identidad: `(int x, long y)` y `(int a, long b)` comparten clave.
- La clave `tuple:(longitud:tipo,...)` permite nesting sin ambigüedad.
- El parser reconoce tipos tupla en returns, parámetros, locals, genéricos y tuplas anidadas.
- Name lookup trata la tupla como estructural y resuelve recursivamente cada elemento.
- La clonación/especialización genérica conserva estructura y nombres sin interpretar `$tuple`
  como una plantilla nominal.

### Layout y ABI Windows x64

- `ProjectObjectLayoutModel` calcula tamaño, alineación y offsets recursivos con la misma regla de
  agregados que los structs.
- MIR descubre todas las claves tupla usadas por firmas/cuerpos, las ordena determinísticamente y
  publica layouts físicos sintéticos.
- Agregados de 1/2/4/8 bytes usan carrier entero directo; los demás usan parámetro indirecto y
  retorno `sret`, igual que structs bajo la ABI x64 de Microsoft.
- La fixture comprueba 8 bytes directos, 16 bytes indirectos y tuplas anidadas de 12/24 bytes.

### Literales y conversiones

- Un literal analiza elementos una vez y en orden textual; su tipo se infiere estructuralmente.
- HIR reutiliza `AggregateInitializer`/`AggregateElement`; MIR y LLVM inicializan storage in-place.
- `ProjectConversionKind::Tuple` clasifica aridad exacta y conversiones recursivas por elemento.
- LLVM derrama el carrier fuente una vez, convierte cada campo y construye el carrier destino sin
  reinterpretar padding ni repetir efectos.

### Deconstrucción

- Asignación `(a, b) = source` y descartes `_` son semánticos, no una reescritura textual.
- La fuente completa se evalúa/materializa antes de calcular cualquier dirección destino.
- Esa secuencia hace seguro `(a, b) = (b, a)` y los destinos aliasados.
- `var (a, _, b) = source` crea locals inferidos en el mismo scope y evalúa la fuente una vez.
- HIR/MIR publican `tuple-deconstruct` y descriptores con offset/tipo/conversión.
- MIR schema sube de 1.16 a 1.17.

## Límites deliberados y dependencias

- `Deconstruct` nominal no se finge como una tupla: requiere resolución de protocolo/método.
- Deconstrucción de elementos owning/move-only se rechaza hasta disponer de bindings move,
  partial drop y drop flags en tandas 13–14.
- Nombres públicos de elementos en NSMETA, consumidor precompilado, debugger y LSP siguen abiertos;
  sus dueños son integración/metadata y tanda 29.
- Acceso `.ItemN`/nombre no se añadió: la observación segura actual es deconstrucción explícita.
- `record struct`, campos promovidos y spreads generales pertenecen a tanda 08.

## Archivos nuevos

- `tests/cc1/c57_ns3_tuples/c57_ns3_tuples.nsproj`: proyecto Language 3 permanente.
- `tests/cc1/c57_ns3_tuples/Main.ns`: ejecución O0 de tipos/literales/conversiones/ABI/deconstrucción.
- Este documento: evidencia completa de la tanda y de sus fronteras.

## Archivos modificados

- `include/nativesharp/AST/AstNodeKind.h`
- `include/nativesharp/AST/TypeSyntax.h`
- `include/nativesharp/Parser/Parser.h`
- `include/nativesharp/Frontend/ProjectTypeConversion.h`
- `include/nativesharp/Frontend/ProjectObjectLayoutModel.h`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/AST/AstNodeKind.cpp`
- `src/AST/TypeSyntax.cpp`
- `src/AST/AstDumper.cpp`
- `src/Parser/Parser.cpp`
- `src/Frontend/ProjectGenericModel.cpp`
- `src/Frontend/ProjectNameLookup.cpp`
- `src/Frontend/ProjectMemberModel.cpp`
- `src/Frontend/ProjectTypeConversion.cpp`
- `src/Frontend/ProjectObjectLayoutModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `tests/unit/Cc1CorrectnessTests.cpp`
- `docs/3.0/TANDA-02-TUPLAS-DECONSTRUCCION-Y-REGISTROS.md`

## Mejora obtenida

NativeSharp 3.0 ya tiene un agregado estructural sin dependencia de runtime administrado. La misma
identidad gobierna firmas, locals, conversiones y ABI; el mismo layout gobierna HIR, MIR y LLVM;
la deconstrucción conserva evaluación única y aliasing; y los dumps permiten auditar cada decisión.
Los casos aún inseguros fallan en frontend con causa concreta en vez de producir código parcial.

## Validación dirigida

- Build exclusivo `nsc`, configuración Debug/O0, 12 jobs: correcto.
- Compilador ejecutado: `A:\nsc-devel\build-clang\bin\nsc.exe`.
- `c57_ns3_tuples`, O0: compila/enlaza y termina con código 42.
- Cobertura de la fixture: nombres sin identidad, nesting, literal, conversión recursiva, returns y
  parámetros Win64 direct/indirect, swap, discard, evaluación única y `var` deconstruido.
- Dump HIR: aggregate tuple, conversion `tuple`, `tuple-deconstruct` y descriptores explícitos.
- Dump MIR schema 1.17: layouts sintéticos deterministas, 8-byte direct, 16/12/24-byte indirect y
  `tuple-deconstruct-element-store`.
- Una negativa dirigida produjo los tres diagnósticos esperados: aridad, fuente no-tupla y binding
  duplicado.
- No se ejecutó CTest, baseline, matriz O1/O2/O3, instalación, despliegue ni remoto.
