# Conversiones de resultados e integración del runtime C/C++

Este registro describe los cambios realizados para compilar los ejemplos de
`Z:\nsc-tutorials\OpenGL-3\ogl`. La implementación continúa: una compilación
correcta de NSC no demuestra por sí sola la cobertura completa de C o C++.

## Commits

| Commit | Alcance |
| --- | --- |
| `172832be2` | Representación y descenso a LLVM de las conversiones de resultados importados, preservando las reglas ordinarias de NativeSharp. |
| `30f4478a4` | Identificación de llamadas C/C++, incluido el seguimiento conservador de punteros a funciones en variables locales. |
| `8d233f72f` | Uso de resultados importados como condiciones y en retornos de lambdas. |
| `3ec868e49` | Conversión de resultados importados en inicializadores de arreglos y colecciones. |
| `387225da4` | Enlace de bibliotecas de importación del UCRT y del runtime C++ encontradas a partir de los includes de sistema configurados. |

## Comprobación y pendientes

- NSC compiló con Clang y se instaló en `C:\native-sharp\bin\nsc.exe`.
- El tutorial 03 superó el error original de conversión de `int` a `uint`.
  Tras corregir el enlace del runtime, los tutoriales 03 y 04 compilaron.
- El tutorial 03 abrió su ventana desde su carpeta de salida y permaneció
  ejecutándose hasta solicitar su cierre. La equivalencia visual no se evaluó.
- No se ejecutó la batería de pruebas de NSC. Las conversiones de `bool`,
  enums y punteros locales todavía requieren comprobación funcional específica.
- En la primera pasada compilaron los tutoriales 01 y 02. Otros proyectos
  registraron errores de código, un conflicto de tipos de GLM o terminaciones
  internas del compilador; falta investigarlos y repetir las compilaciones.
- El seguimiento de punteros locales es conservador cuando no puede demostrar
  el origen de todas sus posibles asignaciones.

## Diagnóstico del conflicto de GLM

El diagnóstico de dependencias de registros distingue ahora una colisión de
identidad, definiciones incompatibles y definiciones múltiples no verificadas.
Esto permite identificar la causa que corresponda al tutorial 17; no cambia
las reglas de aceptación ni da por resuelto el conflicto. Está pendiente
recompilar NSC y repetir el tutorial con el diagnóstico actualizado.

La primera repetición del tutorial 17 informó definiciones múltiples no
verificadas. El diagnóstico incorpora los datos disponibles de cada definición
completa para comparar su identidad, ubicación y layout; la nueva salida aún
requiere recompilarse y verificarse.

El script externo `Z:\nsc-tutorials\OpenGL-3\ogl\build_all.py` también se
actualizó para copiar las DLL transitivas disponibles y los recursos a la
carpeta de cada ejecutable. No forma parte de este repositorio.
