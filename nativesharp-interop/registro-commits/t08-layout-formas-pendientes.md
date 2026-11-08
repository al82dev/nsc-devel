# T08-010/011/020: formas de layout C que NSC todavía no importa

## Propósito

Registrar con evidencia ejecutada qué formas de `struct` y `union` C quedan fuera de la proyección actual, para
que T08-010, T08-011 y T08-020 no se den por cerrados. Este commit sólo agrega sondas exploratorias y
documentación; no modifica código de producción, NSMETA, NSLink ni pruebas existentes.

SCOPE LOCK: [t08-claude-layout-scope-lock.md](t08-claude-layout-scope-lock.md).

## Sondas (`nativesharp-interop/pruebas-tanda-08-layout-clang`)

Archivos: `ProbeAnonymous.h/.ns/.nsproj`, `ProbeNested.h/.ns/.nsproj`, `ProbeFlex.h`, `ProbeFlexHeader.ns/.nsproj`,
`ProbeFlexData.ns/.nsproj`, `ApiProbes.c` y `explorar-formas.cmd`. El script nunca falla por el resultado de una
sonda: imprime una línea `RESULTADO` por sonda y guarda el log de compilación. Acepta las mismas variables que
`validar.cmd`.

Ejecución: build `7b8852b99ca692c0` desde `ceac3f83b`, `A:\al82dev-clang`, O0, log
`A:\nsc-claude-t08-layout-build\logs\formas-1.log`.

| Sonda | Forma C | Resultado |
|---|---|---|
| `ProbeNested` | `struct LGuest` definido dentro de `LGuestHost` | compila, ejecuta y C confirma: SOPORTADO |
| `ProbeFlexHeader` | arreglo flexible: `sizeof` y campo previo | compila, ejecuta y coincide con C: SOPORTADO |
| `ProbeAnonymous` | miembros anónimos C11 (`union { int i; float f; };` y `struct { short x, y; };`) | RECHAZADO |
| `ProbeFlexData` | acceso al miembro de arreglo flexible `data[1]` | RECHAZADO |

La sonda de struct anidado con nombre refuta una inferencia que se había planteado durante la lectura (que
`collectRecords` no lo recogía): HECHO COMPROBADO que funciona.

## Hallazgo 1: miembros anónimos C11 (T08-010, T08-011)

Diagnóstico exacto:

```text
ProbeAnonymous.ns:6:5: error: receiver/type 'type:CLayoutProbeAnonymous::LAnonymous' has no compatible member named 'i'
ProbeAnonymous.ns:7:5: error: receiver/type 'type:CLayoutProbeAnonymous::LAnonymous' has no compatible member named 'x'
ProbeAnonymous.ns:8:5: error: receiver/type 'type:CLayoutProbeAnonymous::LAnonymous' has no compatible member named 'y'
```

Causa en código (HECHO COMPROBADO por lectura; la cadena completa es INFERENCIA hasta implementarla):

- `ClangSession.cpp`, `collectRecords`, recorre `record->fields()`: el miembro anónimo llega como un campo sin
  nombre de tipo record anónimo; los miembros promovidos (`IndirectFieldDecl`) no se capturan.
- `CppSemanticRecord.cpp`, `projectCppSemanticInstanceField`: rechaza todo campo con `field.name.empty()`.

Corrección necesaria: capturar los miembros promovidos con offset absoluto (offset del miembro anónimo más offset
interior, incluidos anidamientos) y proyectarlos, permitiendo el solapamiento de los miembros de una union anónima
dentro de un struct. Para que un consumidor precompilado los vea, el catálogo C de NSMETA tendría que llevarlos;
eso es un cambio de contenido de metadata protegida. Estado: NO IMPLEMENTADO; requiere decidir cómo se representan
en NSMETA antes de implementarlo de forma completa.

## Hallazgo 2: miembro de arreglo flexible (T08-010)

```text
ProbeFlexData.ns:5:5: error: receiver/type 'type:CLayoutProbeFlexData::LFlex' has no compatible member named 'data'
```

Causa en código (HECHO COMPROBADO por lectura): `describeType` no da tamaño a un arreglo incompleto y
`projectCppSemanticInstanceField` exige `field.type.sizeBits`; el campo se omite sin diagnóstico. `sizeof(LFlex)` y
los campos previos sí coinciden con C. Estado: NO IMPLEMENTADO. La representación de arreglos de límite
desconocido pertenece también a T08-013/T08-016; no se implementa aquí para no invadir esos ítems sin decisión.

## Hallazgo 3: `sizeof(Tipo C)` en consumidor sólo-metadata (T08-020)

Con `Metadata.nsproj` (sin encabezado, sólo la biblioteca precompilada):

```text
Consumer.ns:3:16: error: unknown identifier 'LNatural' in callable/initializer context
Consumer.ns:4:17: error: unknown identifier 'LNatural' in callable/initializer context
```

En el mismo archivo `offsetof(LNatural, d)` resolvió y `Access.ns` usa `LNatural*` sin error. Causa (HECHO
COMPROBADO por lectura de `PostfixIterationMachine::startAtomicUnary` y `Parser::isKnownTypeName`): el parser sólo
reconoce `sizeof(T)` como forma de tipo si `T` está entre sus nombres de tipo conocidos, y los tipos C que llegan por
metadata no se agregan a esa lista; `offsetof` siempre analiza un tipo. `registro-commits/validacion-bitfields-c.md`
ya había observado que los nombres C explícitos se rechazan sin encabezado. Corregirlo toca la publicación de
nombres de la superficie precompilada (`src/Driver/PrecompiledSurface`, `ProjectCompilation.cpp`), compartida con
el trabajo de identidad y caché: HALLAZGO NO AUTORIZADO, no implementado. La prueba de layout mide el tipo
reconstruido con `sizeof(*p)` y lo declara como limitación.

## Estado

- No cierra ningún ítem. T08-010, T08-011, T08-012 y T08-020 siguen abiertos.
- Sin código de producción; sin recursión; sin escrituras fuera de `A:`.
