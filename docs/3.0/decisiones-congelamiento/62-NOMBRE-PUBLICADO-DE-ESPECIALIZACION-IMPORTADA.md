# Nombre publicado de una especialización importada

## SCOPE LOCK

- **Objetivo:** hacer que `NativeSharp.Transfer` pueda enlazar
  `NativeSharp.Net.Dns.TryResolve` cuando su firma contiene `Span<Endpoint>`.
- **Naturaleza:** corrección del productor de identidad genérica precompilada.
- **Invariante:** la misma clave canónica de especialización debe conservar el nombre físico que ya publicó
  NSMETA, aunque la fuente pública reconstruida escriba el argumento con otra calificación.
- **Revisión de partida:** `5123e6d8cf2c5d1392924bc235c4e3816917ef45` sobre `development`, con árbol limpio.
- **Evidencia:** NSMETA publica `Span__g1__Endpoint__h6b87ca48b3b30ce5`, mientras el snapshot de la misma
  biblioteca reconstruye `Span__g1__NativeSharp_Net_Endpoint__h6b87ca48b3b30ce5`.
- **Archivos de código autorizados:** `ProjectGenericModel.h`, `ProjectGenericModel.cpp` y
  `Generics/SpecializationExecutor.cpp`.
- **Archivos protegidos:** bibliotecas, pruebas, fixtures, NSMETA, formato de snapshots, versiones ABI,
  parser, semántica del lenguaje, linker y NSC Builder 3.
- **Validación autorizada:** compilar únicamente el target `nsc`; la recompilación de bibliotecas y la prueba
  de `NativeSharp.Transfer` quedan reservadas al usuario.
- **Acciones prohibidas:** suites, baterías, minibaselines, baseline, instalación, deploy, `push`, `pull` y
  `fetch`.
- **Terminación:** el modelo conserva el nombre publicado para materializar una especialización importada sin
  declaración concreta; `nsc` compila; el cierre funcional queda pendiente de la recompilación externa de
  `NativeSharp.Net` y `NativeSharp.Transfer`.

## Causa

`seedImportedSpecializationsForBuild` sólo registra especializaciones que poseen una declaración concreta en
la metadata importada. Eso es correcto para no fingir que un tipo inexistente está disponible, pero pierde el
nombre físico de una especialización externa usada por una firma pública. Al recompilar la fuente pública para
capturar el snapshot, el modelo vuelve a formar el nombre desde la escritura del argumento y puede producir un
nombre distinto para la misma clave canónica.

## Corrección

Las especializaciones de tipo importadas sin declaración concreta no se marcan como materializadas. Únicamente
se registra su nombre publicado como pista asociada a la clave canónica. Si el compilador materializa después
esa clave porque la plantilla sí está disponible, usa la pista en lugar de reconstruir el nombre desde el texto
de la fuente. La especialización continúa pasando por el flujo ordinario de análisis y materialización.

## Preservación de contratos

- No cambia el algoritmo que calcula la clave canónica ni su hash.
- No cambia el mangling de especializaciones propias ni de especializaciones sin metadata publicada.
- No cambia NSMETA, `.nsproducts`, su esquema ni sus versiones.
- No crea alias ni conserva dos claves de tipo para el mismo struct.
- No vuelve utilizable una plantilla ausente: la pista sólo decide el nombre después de resolver la plantilla y
  demandar su materialización.

## Validación

- `cmake --build build-clang --target nsc --parallel 16`: código de salida 0.
- `git diff --check`: sin diagnósticos.
- Comprobación de líneas mayores de 120 caracteres en los cuatro archivos modificados: sin coincidencias.
- No se ejecutaron pruebas, suites, baterías ni baseline.
- La comprobación funcional de `NativeSharp.Transfer` queda pendiente de que el usuario reconstruya primero
  `NativeSharp.Net` y después su consumidor con este compilador.
