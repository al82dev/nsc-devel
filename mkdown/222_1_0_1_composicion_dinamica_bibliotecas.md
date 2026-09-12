# 222 — Composición dinámica de bibliotecas / Dynamic library composition

## Resultado / Outcome

**ES.** La regresión de bibliotecas precompiladas cubre ahora un diamante dinámico real en O0:
dos DLL superiores consumen la misma DLL genérica desde directorios físicos distintos y un EXE
consume ambas. El ejecutable importa sólo las dos DLL superiores, el empaquetador sigue las dos
aristas transitivas, reconoce que ambas copias de la dependencia son byte-idénticas y publica una
sola copia. La cadena completa ejecuta con código 42.

**EN.** The precompiled-library regression now covers a real O0 dynamic diamond: two upper DLLs
consume the same generic DLL through different physical directories and one EXE consumes both. The
executable imports only the upper DLLs, packaging follows both transitive edges, recognizes the two
dependency copies as byte-identical, and publishes exactly one copy. The complete chain exits with
42.

## Topología / Topology

```text
                         +-> CompositeNative.dll ----+
NativeMath.dll/lib/meta -|                            |-> DiamondConsumer.exe
                         +-> CompositeSibling.dll ---+
```

Cada DLL superior materializa `Box<long>` y llama miembros públicos de `NativeMath`. Esto obliga a
la composición a conservar identidades genéricas, RTTI/descriptores, helpers de runtime y símbolos
auxiliares sin convertirlos en definiciones fuertes duplicadas dentro del enlace final.

Each upper DLL materializes `Box<long>` and calls public `NativeMath` members. Composition therefore
must preserve generic identities, RTTI/descriptors, runtime helpers, and auxiliary symbols without
turning them into duplicate strong definitions at the final link.

## Implementación cubierta / Covered implementation

La lógica de producción ya estaba concentrada en dos funciones del driver y se conserva como única
autoridad:

- `verifyLinkedRuntimeLibraries`: parte de imports directos del PE y expande la clausura leyendo las
  import tables de cada runtime declarado.
- `packageRuntimeClosure`: parte exclusivamente de runtimes declarados, sigue DLL hermanas por nombre
  exacto, agrupa por basename case-insensitive y acepta rutas distintas sólo cuando los bytes son
  iguales.

The production logic remains centralized in `verifyLinkedRuntimeLibraries` and
`packageRuntimeClosure`. This delivery does not duplicate that algorithm. Instead, it adds the
missing diamond acceptance case that would fail if path identity were confused with runtime
identity or if equivalent dependencies were copied twice.

## Cambios en el corpus / Corpus changes

`tests/unit/Stage46NativeLibrariesTests.cpp` agrega:

1. `CompositeSiblingProducer`, una segunda `SharedLibrary` que referencia `NativeMath` y materializa
   la misma especialización `Box<long>`;
2. `DiamondConsumer`, que contiene dos `LibraryReference` superiores y ninguna referencia directa
   a `NativeMath`;
3. compilación O0 única para el caso nuevo;
4. inspección PE que exige imports directos de `CompositeNative.dll` y `CompositeSibling.dll`, pero
   no de `NativeMath.dll`;
5. inspección del directorio de salida que exige las dos DLL superiores y exactamente un archivo
   `NativeMath.dll`;
6. ejecución final con código 42.

The same test adds a second shared-library producer, a source-separated diamond consumer, a single
O0 compilation, PE import inspection, exact package-content checks, and execution with exit code 42.

## Casos acumulativos ya presentes / Existing cumulative cases

El cierre se apoya además en regresiones existentes del mismo archivo:

- DLL A → DLL B → EXE;
- clases opacas, virtuales e interfaces entre DLLs;
- delegates con code/context/destructor;
- excepciones tipadas, RTTI y `finally` cruzando DLL;
- dependencia privada que permanece transitiva;
- conflicto de dos DLL distintas con el mismo basename;
- import library que intenta suplantar NSMETA;
- DLL cuya export table no corresponde al contrato;
- módulos duplicados y versiones NSMETA incompatibles rechazados antes del enlace.

Closure also relies on existing regressions for linear DLL chains, opaque classes and dispatch,
delegate cells, typed exceptions/RTTI/finally, private transitive dependencies, conflicting same-name
DLLs, forged import libraries, export/metadata mismatch, duplicate modules, and incompatible NSMETA.

## Decisiones / Decisions

1. **Identidad lógica por basename + contenido.** Dos rutas no crean dos runtimes si el loader de
   Windows sólo puede resolver un nombre y los bytes son idénticos.
2. **Conflicto temprano si difieren.** Dos contenidos distintos con el mismo basename nunca se
   eligen por orden de PATH.
3. **No aplanar DLLs.** El EXE no importa dependencias privadas que pertenecen a otra DLL.
4. **Clausura explícita.** No se escanea PATH ni directorios arbitrarios; sólo se siguen runtimes
   declarados y siblings nombrados por import tables.
5. **O0 para el nuevo diamante.** Respeta el carril de iteración actual y evita duplicar la pasada
   O2 histórica.

1. **Logical identity is basename plus content.** Different paths do not create different runtimes
   when the Windows loader resolves one name and bytes are identical.
2. **Early conflict for different bytes.** PATH order never chooses between different same-name DLLs.
3. **Do not flatten DLLs.** The EXE does not import another DLL's private dependencies.
4. **Explicit closure.** No PATH or arbitrary-directory scan occurs.
5. **O0 for the new diamond.** This follows the current iteration lane without duplicating the
   historical O2 pass.

## Verificación durante iteración / Iteration verification

El escenario se incorpora al baseline final, no se ejecuta en esta entrega. Después del commit se
reconstruye únicamente `nsc` desde una carpeta limpia.

The scenario joins the final baseline and is not executed during this delivery. Only `nsc` is
rebuilt from a clean directory after the commit.

## Commit de referencia / Reference commit

**Título ES:** Cierra la composición dinámica de bibliotecas

**Title EN:** Complete dynamic library composition

**Descripción ES:** Agrega un diamante DLL O0 con especializaciones genéricas compartidas, comprueba
imports transitivos y deduplicación por contenido y ejecuta la cadena sin duplicar runtime ni
símbolos auxiliares.

**Description EN:** Add an O0 DLL diamond with shared generic specializations, verify transitive
imports and content-based deduplication, and execute the chain without duplicating runtime or
auxiliary symbols.
