# NativeSharp 1.0.1 — documentación vigente

Esta carpeta es la edición autocontenida de la documentación correspondiente a la línea de
desarrollo **NativeSharp 1.0.1**. No es un archivo cronológico de decisiones: describe el lenguaje,
el compilador, el ABI, la depuración y la biblioteca tal como deben entenderse al programar con la
versión actual.

Mientras no exista un baseline 1.0.1 certificado, cada compilador de esta línea se identifica como:

```text
nsc 1.0.1-dev+build.<UTC>.<id> (NativeSharp 1.0.1, stage 67)
```

El sufijo `-dev` significa que el producto continúa evolucionando. El identificador de build evita
confundir dos ejecutables construidos desde estados distintos. La carpeta `1.0.1` identifica la
línea que la documentación explica; no convierte por sí sola un build de desarrollo en release.

## Ruta de lectura

1. [VERSION.md](VERSION.md) fija qué significa cada número visible y qué puede afirmarse sin un
   baseline nuevo.
2. [Roadmap hacia 2.0](ROADMAP_TO_2_0.md) gobierna los snapshots `1.0.1-dev-NNN`, el cierre de
   1.0.1, la evolución compatible de 1.x y la puerta explícita para cambios incompatibles 2.0.
3. [Items finales de NativeSharp 2.0](NATIVESHARP_2_0_ITEMS_FINAL.md) divide la implementación
   pendiente en tandas y commits verificables, y conserva su progreso acumulativo.
4. [Estado actual del lenguaje](CURRENT_STATE.md) resume la superficie disponible para escribir y
   portar aplicaciones nativas.
5. [Especificación del lenguaje](language/NativeSharp_Language_Specification_1_0_1.md) es la
   autoridad normativa para sintaxis, tipos, ownership, excepciones, genéricos, ABI y ejecución.
6. [Gramática](language/NativeSharp_Grammar_1_0_1.ebnf) define la superficie sintáctica completa.
7. [Mapa de conformidad](language/NativeSharp_1_0_1_conformance.map) conecta cada cláusula con su
   implementación y pruebas.
8. [Contrato del compilador](compiler/NativeSharp_Compiler_Contract_1_0_1.md) define diagnósticos,
   artefactos, paquetes, compatibilidad y fallos controlados.
9. [Contrato de depuración](debug/NativeSharp_Debug_Artifact_Contract_1_0_1.md) gobierna CodeView,
   PDB, líneas, variables, tipos, closures y excepciones nativas.
10. [Referencia extensa en español](book/NativeSharp_El_Lenguaje_de_Programacion_ES.md) explica el
   lenguaje con programas sustanciales, modelos de costo y casos de uso.
11. [Cambio desde 0.9](CHANGELOG.md) resume la evolución que originó esta edición.

## Qué documentación es actual y qué es histórica

Todo lo situado bajo `docs/1.0.1` pertenece a esta edición. Los documentos de `mkdown/` conservan la
historia de ingeniería, diagnósticos, decisiones y commits; son evidencia útil, pero no son el punto
de entrada para aprender el producto actual. Los archivos originales de `spec/` y `book/` se
mantienen para conservar las ediciones anteriores de las que se derivó esta copia.

Cuando se instala NSC mediante CMake, esta carpeta viaja a:

```text
<prefix>/share/nativesharp/docs/1.0.1
```

`nsc --verify-installation <prefix>` exige el índice, el contrato de versión, las especificaciones
principales y la referencia extensa. Una instalación que sólo contiene el ejecutable ya no puede
presentarse como distribución completa de 1.0.1.

## Reglas de lectura para quien viene de C#

NativeSharp comparte sintaxis y nombres familiares con C#, pero no es un CLR reducido. La autoridad
es esta documentación y no la existencia de una construcción en C#. En particular:

- ownership, préstamos, RAII y destrucción determinista forman parte del contrato nativo;
- structs, punteros, delegates, callbacks y excepciones obedecen el ABI nativo de Windows x64;
- los genéricos se materializan AOT y las bibliotecas precompiladas publican la clausura necesaria;
- `params`, argumentos nombrados, métodos de extensión, interpolación, `??`, `??=` y `?.` no se
  presuponen ni se necesitan para considerar completo un port nativo;
- una adaptación explícita a una API nativa no es automáticamente un defecto del compilador.

## Estado de validación

Esta edición fue generada durante desarrollo y no declara un baseline 1.0.1 nuevo. La comprobación
de este cambio se limita a coherencia de versiones, enlaces internos, empaquetado e identidad de
build. La certificación posterior deberá registrar la identidad exacta del compilador y sus
resultados O0 sin reescribir retroactivamente este estado.

---

# NativeSharp 1.0.1 — current documentation

This directory is the self-contained documentation edition for the **NativeSharp 1.0.1**
development line. It is current product documentation, not a chronological decision archive.
Until a new baseline is certified, compiler binaries use
`1.0.1-dev+build.<UTC>.<id>`. Installation places this tree under
`share/nativesharp/docs/1.0.1`, and installation verification requires its primary entry points.

NativeSharp is a native language with its own ownership, ABI, AOT generics, exception, and library
contracts. C# familiarity helps with syntax, but unsupported C# conveniences must not be confused
with missing native functionality. The normative language, compiler, and debug contracts in this
directory are the authority for the 1.0.1 development line.
