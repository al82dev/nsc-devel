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
| `9e93e820b`, `1d9afb924` | Diagnósticos del conflicto entre definiciones de registros C/C++. |
| `b2d8aab66` | Comparación estricta de registros C++ simples sin hash ODR. |
| `dba12c502` | Conservación de los inicializadores dinámicos de namespace durante la reconstrucción del modelo. |

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
  internas del compilador; falta repetir las compilaciones afectadas.
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
completa para comparar su identidad, ubicación y layout. La salida mostró el
mismo USR, tamaño, alineación y posición de origen en tres observaciones, sin
hash ODR disponible.

Se agregó una vía de comparación para registros C++ compuestos exclusivamente
por datos cuando Clang no proporciona hash ODR: exige identidad y procedencia
iguales, mismo destino, layout, rasgos y miembros capturados equivalentes, y
excluye métodos, bases y campos estáticos. El tutorial 17 superó ese conflicto
con la nueva regla y después presentó una violación de acceso; su repetición
con la corrección de duración de nodos terminó sin esa violación de acceso.

El script externo `Z:\nsc-tutorials\OpenGL-3\ogl\build_all.py` también se
actualizó para copiar las DLL transitivas disponibles y los recursos a la
carpeta de cada ejecutable. No forma parte de este repositorio.

## Duración de inicializadores dinámicos

El tutorial 11 terminaba con una violación de acceso al reconstruir el modelo
de namespaces después de deducir plantillas C++. La pila y la inspección de
memoria mostraron que el primer nodo insertado en `Main` ya había sido liberado:
su propietario era el modelo temporal de namespaces, aunque el árbol de
sintaxis conservaba el puntero.

Los nodos de inicialización se crean ahora en la arena de la unidad que contiene
`Main`, cuya duración abarca las reconstrucciones del modelo. NSC se compiló e
instaló con esta corrección. Al repetir el tutorial 11 dejó de terminar por
violación de acceso y mostró diagnósticos de tipos del código NativeSharp;
el tutorial todavía no compila.

## Repeticiones posteriores de los tutoriales

- **Tutorial 15:** con el compilador corregido ya no termina con violación de
  acceso. La compilación falla por inicializaciones de uint y operaciones
  numéricas en el código NativeSharp. Registro:
  Z:/nsc-tutorials/OpenGL-3/ogl/results/tutorial15_lightmaps-O3.log.
- **Tutorial 17:** superó el conflicto de GLM y ya no termina con violación
  de acceso. La compilación sigue fallando: hay errores de tipos y usos de
  AntTweakBar que no se pueden resolver; no se encontró AntTweakBar.h en
  Z:/sdk. Esto no demuestra que la aplicación funcione.
- **Tutoriales 09 AssImp y 18 billboards:** con la misma corrección ya no
  terminan con violación de acceso. La compilación sigue fallando por
  errores de tipos en el código de los ejemplos. Los registros están en
  Z:/nsc-tutorials/OpenGL-3/ogl/results, con sus nombres respectivos.
- No se ejecutó la batería de pruebas de NSC.
