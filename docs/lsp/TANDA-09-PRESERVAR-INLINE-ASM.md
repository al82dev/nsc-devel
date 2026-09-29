# Tanda 9: preservar inline assembly

## Objetivo

Evitar que Format Document cambie bytes dentro de un bloque asm, cuyo cuerpo es una entrada física
para el ensamblador.

## Cambios

- El formateador reconoce el identificador contextual asm seguido de llave fuera de comentarios,
  strings y caracteres.
- Un documento con inline assembly no produce TextEdit.
- La prueba focalizada conserva incluso una línea de ensamblador deliberadamente sin sangría.

## Mejora

El formato permanece conservador: es preferible no editar un archivo mixto que modificar la
semántica de ensamblador válida.

## Verificación

Construcción Debug/O0 y ejecución exclusiva de nsc_language_service_formatting_tests, seguida del
smoke nsc-lsp. Sin baseline ni CTest general.
