# Tandas de interoperabilidad para los proyectos OpenGL

## SCOPE LOCK

- Objetivo: resolver secuencialmente los impedimentos conocidos para compilar
  y ejecutar los proyectos de Z:/nsc-tutorials/OpenGL-3/ogl usando las APIs
  C/C++ reales, sin bindings ni workarounds.
- Fuente del compilador: A:/nsc-devel. Se preservan las reglas ordinarias de
  NativeSharp y los tutoriales originales en Z:/project-tutorials.
- Cada tanda se termina, documenta y compromete antes de compilar NSC y pasar
  a la siguiente. Si la compilación falla, se corrige en la misma tanda.
  NSC se construye exclusivamente en Release con optimización O3.
- No ejecutar tests unitarios, baselines ni baterías de pruebas. Al terminar
  la octava tanda se compilan y ejecutan los proyectos OpenGL; ante errores
  se vuelve a la tanda correspondiente.
- No instalar ni sustituir C:/native-sharp: el usuario instala. Los ejecutables
  exitosos deben tener sus DLL y recursos en sus respectivas carpetas.
- Cierre: proyectos compilados y ejecutados con comportamiento comprobado o
  bloqueos reales registrados; la compilación de NSC por sí sola no certifica
  el comportamiento de los demos.

## Estado de las tandas

| Tanda | Alcance | Estado |
| --- | --- | --- |
| 1 | Condiciones de valores C/C++ importados | Implementada y NSC compilado; demo pendiente |
| 2 | Callbacks C/C++ | Implementada; compilación de NSC pendiente |
| 3 | Constante cero como puntero en llamadas C | Commit 4cafd1113 existente; cotejo y compilación pendientes |
| 4 | Expresiones numéricas | Pendiente |
| 5 | Declaraciones de campos | Pendiente |
| 6 | Fuentes y punto de entrada de proyectos auxiliares | Pendiente |
| 7 | Dependencia real AntTweakBar | Pendiente |
| 8 | Salidas, DLL y recursos | Pendiente |

## Tanda 1: condiciones de valores importados

- **Problema observado:** el tutorial 12 usa en condiciones las variables C
  GLEW_AMD_seamless_cubemap_per_texture y GLEW_ARB_debug_output, ambas
  GLboolean; el analizador exige bool.
- **Causa:** la conversión a condición reconocía resultados de llamadas
  C/C++, pero no otras lecturas directamente marcadas con cppCOrigin.
- **Cambio:** en condiciones, !, && y ||, clasificar lecturas C/C++
  numéricas o punteros mediante la conversión extranjera a bool y
  conservarla al construir HIR. Los valores ordinarios de NativeSharp no
  reciben esta excepción.
- **Validación:** NSC compiló y el ejecutable arrancó con build
  4ba4f7e577fdcfed. No se ejecutaron pruebas ni se compiló el tutorial;
  su comportamiento queda pendiente de la pasada posterior a la tanda 8.

### Reconstrucción Release/O3

A indicación del usuario, se revisaron los comandos efectivos: la
configuración anterior incluía /O2 seguida de /clang:-O3, por lo que mi
afirmación previa de que necesariamente compilaba en O2 fue incorrecta.
Se reconfiguró build-clang con NSC_HOST_OPTIMIZATION=O3 y sin /O2 en las
opciones Release. Los 641 comandos C/C++ observados contienen /clang:-O3
como única opción de optimización. NSC se reconstruyó y arrancó con
build c12325b356ca1239. No se ejecutaron baterías ni ejemplos.

## Tanda 2: callback con dirección de función

- **Problema observado:** tutorial12_extensions pasa &DebugOutputCallback a
  glDebugMessageCallbackARB; NSC rechaza el grupo de funciones antes de
  compararlo con el tipo de callback del parámetro.
- **Causa:** el contexto que permite resolver un grupo de funciones se
  recibía en el análisis de la expresión, pero el operador & no lo
  transmitía a su operando.
- **Cambio:** cuando el contexto admite un grupo de funciones, & conserva
  ese grupo para que el enlazador de callbacks existente seleccione la firma
  y compruebe la convención. El operador de dirección de variables sigue
  por su ruta anterior.
- **Validación:** revisión estática y git diff --check. Pendiente
  compilar NSC; no se ha comprobado el tutorial.
