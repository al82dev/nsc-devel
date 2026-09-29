# Tanda 2: parser y modificadores

## Objetivo

Permitir que funciones, constantes y variables declaradas en namespace conserven accesibilidad `public` o `internal` en el AST. Hasta esta tanda el parser consumía esos modificadores, pero después rechazaba cualquier declaración que no fuera un tipo.

## Cambios

- `parseGlobalDeclaration` comparte ahora la cola de accesibilidad entre tipos y declaraciones de valor.
- Una `FunctionDeclaration` o `GlobalVariableDeclaration` recibe tanto `tokenKind` como el modificador correspondiente.
- Una declaración sin modificador conserva `TokenKind::Invalid`; la semántica posterior aplicará `internal` como valor predeterminado.
- No cambia el parser procedural histórico ni el formato de los cuerpos.

## Archivos modificados

- `src/Parser/Parser.cpp`: permite accesibilidad sobre funciones y almacenamiento de namespace.
- `tests/unit/NamespaceSyntaxTests.cpp`: cubre funciones públicas, constantes internas y accesibilidad predeterminada.

## Archivos nuevos

- Este documento, para registrar contrato, archivos y verificación del commit.

## Mejora

El AST ya puede expresar la superficie pública real de una función libre sin convertirla en miembro de un tipo. Esta información será consumida por el modelo multiparchivo y posteriormente por NSMETA.

## Verificación específica

- Construcción incremental del objetivo `nsc`.
- Construcción y ejecución exclusiva de `nsc_namespace_syntax_tests`.
- Resultado: `nsc_namespace_syntax_tests` — PASS, 20 checks.
- No se ejecutó baseline, CTest general, fixtures ni niveles distintos de O0.
