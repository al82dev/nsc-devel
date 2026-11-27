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

## Seguimiento estático: punteros locales y bool

- La procedencia de un puntero a función local se decide en
  src/Frontend/Bodies/CallAnalyzer.cpp, en el bloque que establece
  cppForeignCallResult para una invocación de delegate. Recorre toda la
  sintaxis del callable y solo acepta una asignación si su expresión
  derecha ya está en possibleDelegateSources.
- AssignmentAnalyzer.cpp incorpora esa fuente al analizar la asignación.
  Por lo tanto, una asignación posterior que todavía no fue analizada
  impide acreditar la procedencia extranjera incluso si su destino fuera
  otra función importada. Es un límite de la implementación actual,
  no una comprobación de ejecución. Resolverlo exige considerar ramas,
  ciclos, alias y destinos NativeSharp sin relajar sus conversiones.
- OopLlvmFieldsAndConversions.cpp, convertScalar, genera comparación
  distinta de cero para convertir enteros y flotantes a bool; los punteros
  extranjeros se tratan en OopLlvmExpressionMachine.cpp. Esta lectura no
  sustituye comprobaciones funcionales de los casos ni de las enumeraciones.

## Enumeraciones: dato de origen pendiente

- ForeignCallResultConversion.h reconoce una enumeración C o una
  enumeración C++ sin ámbito como origen numérico; su destino debe
  ser un tipo numérico incorporado o bool. El clasificador no contempla
  por esa vía un enum como destino en la revisión anterior.
- La marca cppForeignCallResult de ExpressionResult registra si el resultado
  viene de una llamada C/C++, pero no distingue el modo de lenguaje usado
  para importar el encabezado. CppSessionOptions sí guarda ese modo;
  CppLanguageLinkage describe el enlace de la función, que no es
  equivalente al modo de lenguaje de la sesión.
- **Pendiente:** determinar el contrato de conversión hacia enums C y C++
  importados y cómo preservar el origen requerido hasta semántica y HIR.
  Esta inspección no demuestra que todas las conversiones a enums fallen:
  el clasificador ordinario también interviene antes de esta vía.

## Conversión de resultados importados a enum C

- **Causa:** el clasificador ForeignNumeric rechazaba un destino enum C
  incluso cuando Clang ya había acreditado su tipo entero y layout.
- **Cambio:** classifyForeignCallNumericResult acepta valores aritméticos
  procedentes de una llamada importada para un enum C completo y válido.
  El descenso existente convierte al entero subyacente del enum. Los enums
  C++ y las conversiones ordinarias de NativeSharp quedan fuera del cambio.
- **Alcance:** src/Frontend/ForeignCallResultConversion.h; el clasificador
  solo se invoca como alternativa para resultados de llamadas C/C++.
- **Validación:** revisión estática del clasificador y del descenso.
  Pendientes la compilación y la comprobación funcional de esta tanda.
  El seguimiento de punteros locales todavía tiene el límite anterior.

### Construcción e instalación de la tanda de enum C

NSC compiló con la corrección del commit 2031f69ff y se instaló en
C:/native-sharp/bin/nsc.exe. El ejecutable construido y el instalado tienen
SHA-256 ed68451c480d1387ff7423035c400350bb81cb64ea6678e90888305085986a4a;
el compilador informa build hash 6cb479b3ad7b28dd. La compilación de los
ejemplos O3 está en curso. No se ejecutó la batería de pruebas de NSC
ni se ha verificado funcionalmente la nueva conversión a enum C.

## Campos C++ con punteros multinivel a records

- **Síntoma:** el tutorial 09 AssImp, con NSC build 6cb479b3ad7b28dd,
  informó que no podía importar aiScene.mMeshes de tipo aiMesh **.
- **Causa estática:** projectCppSemanticInstanceField recorría los
  punteros intermedios de campos C++, pero solo cuando la base era escalar.
  Para una base record, la búsqueda de identidad recibía otro puntero.
- **Cambio:** permite recorrer punteros intermedios de campos C++ con base
  record, conservando las comprobaciones existentes de calificadores,
  profundidad e identidad del record. No depende de Assimp.
- **Validación:** pendiente recompilar NSC y repetir el tutorial. El campo
  mTextureCoords, arreglo de punteros, es otro diagnóstico y no forma parte
  de este cambio. Los errores de tipos NativeSharp también siguen pendientes.

### Compilación e instalación de punteros multinivel

- La pasada O3 anterior terminó con 4 de 28 proyectos compilados usando
  build 6cb479b3ad7b28dd. Los registros individuales están en
  Z:/nsc-tutorials/OpenGL-3/ogl/results.
- NSC compiló con el commit 3140b8644 y se instaló con build hash
  9b25b3dae9c2948e. Los ejecutables compilado e instalado coinciden en
  SHA-256 b5705030e344e082359a90c6cd6ff6f7974899f7ca0968545b405ca3cecaabc3.
- Con build 9b25b3dae9c2948e, la recompilación O3 de tutorial09_AssImp
  ya no informa el error de aiScene.mMeshes. El proyecto sigue sin compilar:
  permanece el error de mTextureCoords y los errores de tipos NativeSharp.
  Esto verifica que desapareció ese diagnóstico, no la ejecución del demo.

## Campos C++ que son arreglos de punteros

- **Síntoma:** tutorial09_AssImp informa que aiMesh.mTextureCoords,
  declarado como aiVector3D *[8], no tiene mapeo.
- **Causa comprobada:** projectCppSemanticInstanceField no invocaba el
  mapeador de arreglos para un campo C++ cuyo elemento es un puntero.
- **Cambio:** permite consultar projectCArrayField para arreglos de punteros
  sin calificar de un record C++, incluso si su dueño no es transportable
  por valor. Se conservan la verificación de identidad del tipo apuntado,
  las dimensiones, el acceso al campo y las comprobaciones de layout.
  No se cambia la construcción ni la copia del objeto dueño.
- **Pendiente:** construir NSC y recompilar Assimp. No se ha demostrado que
  el campo se importe ni que su acceso funcione.

### Recompilación de Assimp tras el arreglo de punteros

NSC compiló con el commit dba0c402e y se instaló con build hash
7331717062c4d14d. Los binarios compilado e instalado coinciden en SHA-256
1b6c6dc12a105972c68ac98829133691283f91ac33482226a7a3d03cf575bae4.
La compilación O3 de tutorial09_AssImp ya no reporta los errores de
mMeshes ni mTextureCoords. Permanece fallida por seis diagnósticos de
tipos NativeSharp en texture.ns y tutorial09_AssImp.ns. No se ejecutó el
demo y la ausencia de esos diagnósticos no demuestra su comportamiento.

## Asignaciones posteriores de punteros locales

- **Problema:** CallAnalyzer examinaba todas las asignaciones del puntero,
  pero las posteriores a la llamada aún no estaban registradas y anulaban
  la procedencia extranjera incluso cuando no podían ejecutarse antes.
- **Cambio:** cuando la llamada y la escritura aparecen en sentencias
  distintas del bloque principal, la escritura es posterior y el cuerpo
  no contiene bucles, lambdas, suspensión, excepciones, ensamblador ni
  iteración estática, esa escritura posterior no invalida la llamada
  anterior. Las fuentes anteriores siguen comprobándose; las llamadas a
  destinos NativeSharp no reciben la conversión extranjera por este cambio.
- **Límite:** este análisis no resuelve el flujo general en bucles,
  lambdas ni sentencias compartidas. No se considera terminado el soporte de
  punteros locales.
- **Validación:** revisión estática; pendientes compilación de NSC
  y comprobación funcional de esta ruta.

### Construcción del análisis de punteros locales

NSC compiló con el commit 942656b1f y se instaló con build hash
cc0ef4d583068c72. El binario construido y el instalado coinciden en SHA-256
7d4943b0b26d429b3daca5a0c831633cd21364a484c6ddabfd5991a10d64bbf8.
La pasada O3 de ejemplos se interrumpió a solicitud del usuario: él
compilará e instalará en adelante. No hay un resultado completo de esa
pasada. La validación funcional específica de la nueva ruta sigue pendiente.

## Copias entre punteros locales a funciones

- **Problema:** el seguimiento de una llamada a través de un puntero
  local rechazaba una fuente que se hubiera copiado mediante una asignación
  a otra variable local, aunque el destino fuera también un puntero crudo.
- **Cambio:** reconoce esa lectura de valor cuando el destino está ligado
  a otra variable local de tipo puntero a función. La cadena de fuentes
  sigue revisándose: una fuente no extranjera o desconocida no acredita
  el resultado como extranjero.
- **Alcance:** CallAnalyzer.cpp. No se admite una referencia a la variable ni
  se cambia la clasificación de conversiones de NativeSharp.
- **Validación:** revisión estática. Pendientes compilar NSC y
  comprobación funcional específica.

### Estado tras cambio de instrucción

La corrección 707caa452 compiló en A:/nsc-devel/build-clang/bin/nsc.exe,
pero no se instaló. El usuario indicó que él compilará e instalará.
En ese momento el ejecutable de C:/native-sharp correspondía al build
anterior cc0ef4d583068c72. La pasada de ejemplos se detuvo antes de terminar;
los registros parciales no deben tomarse como un resultado completo.

### Instalación posterior realizada por el usuario

El usuario comunicó que instaló el compilador actualizado. Se comprobó
que C:/native-sharp/bin/nsc.exe y A:/nsc-devel/build-clang/bin/nsc.exe
coinciden en SHA-256
eed6d6ebb36b2f9a12e4494780dcdec8160b9eb839d272609e86af4959d4dc4e.
El ejecutable instalado informa build hash 789d6522a621e6ea. Esta
comprobación no equivale a validar los ejemplos ni las conversiones.
