# Tanda correctiva 09: sintaxis actual y efectos implicitos

## Causa raiz

La tanda 7 uso una sola edicion para tres contratos distintos:

1. la edicion efectiva del grafo y de NSMETA;
2. la superficie sintactica que el compilador reconoce;
3. la politica obligatoria de efectos (`unsafe` y propagacion `throws`).

Conservar Language 2 para los efectos historicos evitaba imponer `unsafe`, pero
tambien ocultaba sintaxis valida como `copy struct`. Ese acoplamiento explica el
`NS3002` de T31 que encabeza el reporte de 234 fallos.

## Correccion

- `languageEdition()` conserva el contrato efectivo del grafo.
- `sourceLanguageEdition()` conserva lo que declaro el autor; una omision sigue
  siendo historicamente Language 2.
- `recognizedLanguageEdition()` permite que una omision reconozca la superficie
  actual sin reescribir la procedencia del proyecto.
- `enforcesLanguage3Effects()` solo es verdadero cuando el proyecto declara
  expresamente `<Language>3</Language>`.
- Method bodies transporta por proyecto la edicion reconocida y la politica de
  efectos. Los shards paralelos reciben ambas sin estado global ni recursion.
- Los gates de `unsafe`, `assume`, ensamblador, casts crudos y propagacion
  tipada de errores consultan la politica de efectos; el resto de la semantica
  Language 3 sigue disponible para una fuente implicita.

No se modifico ninguna prueba, fixture ni proyecto historico. La cobertura
nueva vive en `ImplicitLanguageFeatureRecognitionTests.cpp`.

## Evidencia dirigida

Pasaron juntas 4/4 pruebas:

- `nsc.compilation.language-edition-inference`;
- `nsc.compilation.precompiled-language-edition-inference`;
- `nsc.compilation.implicit-language-edition-semantics`;
- `nsc.compilation.implicit-language-feature-recognition`.

T31 dejo de emitir `NS3002` por `copy struct` y alcanzo la siguiente barrera:
el fixture historico intenta copiar
`lib/NativeSharp.Core/BorrowedReference.ns`, archivo eliminado por el commit
independiente `9232bcfc0`. Esa ausencia se corrige en su productor antes de la
bateria completa.

## Estado de validacion

La tanda permanece en curso hasta ejecutar la bateria completa del arbol
NSLink. No se ejecuto baseline, minibaseline ni ningun perfil distinto de O0.
