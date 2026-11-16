# Tanda correctiva 09: sintaxis actual y efectos implicitos

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

## Causa raiz

La tanda 7 uso una sola edicion para tres contratos distintos:

1. la edicion efectiva del grafo y de NSMETA;
2. la superficie sintactica que el compilador reconoce;
3. la politica obligatoria de efectos (`unsafe` y propagacion `throws`).

Conservar las reglas de efectos históricas evitaba imponer `unsafe`, pero
tambien ocultaba sintaxis valida como `copy struct`. Ese acoplamiento explica el
`NS3002` de T31 que encabeza el reporte de 234 fallos.

## Corrección histórica

La corrección de entonces separó el contrato del grafo, la procedencia fuente,
la superficie reconocida y la política de efectos. Los cuerpos y shards
recibían esos datos por proyecto, sin estado global. Las comprobaciones de
`unsafe`, `assume`, ensamblador, casts crudos y propagación tipada consultaban
la política de efectos.

**Contrato vigente:** la selección por edición fue retirada. La sintaxis y
los efectos se determinan con las reglas reales del lenguaje único.

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
