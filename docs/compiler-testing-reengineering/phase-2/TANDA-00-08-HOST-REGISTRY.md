# Tanda 00.08 — Ejecutable host consolidado

## Resultado

La nueva suite introduce un único binario `nsc_component_tests`. Su registro conserva por caso ID, contrato,
componente, capa, perfil, tags y función. Rechaza metadata incompleta e IDs duplicados, ordena por identidad estable
y permite selección explícita por caso, componente o conjunto completo.

La ejecución usa un `TestContext` nuevo por caso, captura excepciones como fallos y emite una línea JSON por
resultado. Invocar sin selector es error de configuración; no existe un éxito implícito con cero pruebas.

## Aislamiento inicial

Esta tanda contiene solamente tres casos sintéticos del framework:

- invariantes del contexto;
- orden y lookup del registro;
- rechazo de IDs duplicados.

No reutiliza casos históricos ni presenta estos fixtures como cobertura de NSC. Los módulos de producto se
incorporarán desde la Ola 1.

## Construcción

`tests/compiler-suite/CMakeLists.txt` permite construir el binario de forma aislada o incluirlo posteriormente en
el árbol principal. Usa C++20, warnings como errores y runtime estático `/MT` (`/MTd` en Debug). La selección se
declara tanto para MSVC/clang-cl como para Clang con frontend GNU mediante `-fms-runtime-lib`, porque `MSVC` no es
verdadero en esa última configuración aunque el target sea Windows.
