# Tanda de validación 05 — Argumentos propios de `lld-link`

## SCOPE LOCK local

- Evidencia: el enlace de `nsc_component_tests.exe` emite
  `ignoring unknown argument '-fuse-ld=lld'`.
- Operación: configuración independiente de la suite desde Builder 3.
- Símbolo responsable: `configure_commands`.
- Archivo funcional autorizado: `nscbuilder3.py`.
- La configuración de construcción del compilador permanece protegida.

## Causa y corrección

Builder 3 ya selecciona `lld-link.exe` mediante `CMAKE_LINKER`. El argumento
`-fuse-ld=lld` pertenece al driver Clang, pero la suite invoca el linker
directamente. Además, simplemente omitirlo dejaría el valor anterior dentro de
un árbol CMake reutilizado.

La configuración de la suite envía ahora `CMAKE_EXE_LINKER_FLAGS` vacío. Esto
retira el argumento inválido y limpia cualquier valor obsoleto del cache sin
cambiar el linker seleccionado.

## Estado

- Manifestaciones visibles: `1/1` corregida.
- Comando de construcción del compilador: sin cambios.
- Configuración y enlace externos: pendientes de la siguiente corrida.
