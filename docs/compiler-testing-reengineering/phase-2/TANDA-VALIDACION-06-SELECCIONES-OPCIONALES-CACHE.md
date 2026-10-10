# Validación 06 — Selecciones opcionales sin estado heredado

## Propósito

Impedir que un árbol de CMake reutilizado conserve un archivador o un NSC de
comparación que el usuario ya retiró de la configuración de Builder 3.

## SCOPE LOCK

- Repositorio: `A:\nsc-devel`.
- Componente: configuración de la suite desde Builder 3.
- Símbolo funcional: `configure_commands`.
- Archivo funcional: `nscbuilder3.py`.
- Comportamiento autorizado: retirar del cache dos selecciones opcionales
  cuando sus campos vigentes están vacíos.
- Protegidos: compilador, CMake de la suite, pruebas, controladores, oráculos,
  manifiestos, los seis archivos preexistentes, `C:`, despliegues y remotos.
- Validación funcional: pendiente de la siguiente configuración del usuario.

## Cadena causal

1. `NSC_COMPILER_SUITE_ARCHIVE_EXECUTABLE` y
   `NSC_COMPILER_SUITE_COMPARISON_NSC_EXECUTABLE` son variables `CACHE FILEPATH`.
2. Builder 3 sólo enviaba esas variables cuando el campo vigente no estaba
   vacío.
3. Omitir una variable no elimina un valor almacenado por una configuración
   anterior del mismo árbol.
4. CMake podía registrar y ejecutar casos opcionales contra herramientas que
   ya no estaban seleccionadas por el usuario.
5. `configure_commands` envía ahora el `-U` exacto cuando cada campo está vacío.

## Cambio

No se añadió fallback ni selección implícita. El comportamiento es binario:

- campo con ruta: se envía `-D<variable>=<ruta>`;
- campo vacío: se envía `-U<variable>`.

La selección obligatoria de NSC, NSLink, LLVM y las demás opciones permanece
idéntica.

## Evidencia estática

- Variables opcionales auditadas en Builder 3: `3/3`.
- Identidad de producto: cerrada en Validación 03.
- Archivador: limpieza explícita incorporada.
- NSC de comparación: limpieza explícita incorporada.
- Pruebas, manifiestos y producto modificados: `0`.
- Recursión añadida: `0`.
- Complejidad: `O(1)` en tiempo y espacio.

## Estado

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE CONFIGURACIÓN PENDIENTE DEL USUARIO`.
