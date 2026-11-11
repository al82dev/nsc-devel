# Vida de vistas asignadas a salidas

Una asignación directa a un parámetro `ref` u `out` no puede almacenar una
vista de un propietario local ni de un propietario move-only consumido por
valor: ambos se destruyen al terminar la función. El análisis rechaza estas
asignaciones cuando puede identificar el origen y conserva la asignación de
una vista que recibe el propio llamador.

La prueba dirigida `nsc.sdk.shared-view-output-lifetime` analiza las familias
Rc y Arc con ambos tipos de salida. Comprueba ocho escapes —propietario local
o consumido— y cuatro controles válidos. Los controles se compilan y ejecutan
en O0 y O2 y deben devolver 42; los escapes solo se analizan. En la copia de
trabajo aislada, la prueba terminó correctamente. La reproducción previa de
asignación directa aceptaba los escapes antes del cambio.

La verificación usó el compilador aislado con otros cambios locales sin
registrar y fuentes del SDK de la copia de trabajo; no demuestra el resultado
de compilar este commit desde un árbol limpio. La regla solo cubre orígenes
identificables de asignaciones directas; el seguimiento entre llamadas,
orígenes desconocidos, campos y salidas compuestas se investiga por separado.
No se ejecutó una batería completa ni se desplegó el SDK.

La regresión se amplió posteriormente con vistas guardadas primero en una
variable y con expresiones condicionales, tanto para propietarios locales
como para propietarios consumidos; también se mezcló un origen del llamador
con otro local en una condicional. En el árbol de trabajo actual se analizaron
32 casos: 28 rechazos esperados y cuatro controles aceptados; los controles
completaron ocho ejecuciones O0/O2 con salida 42. El registro está en
`A:/nsc-fixes-evidencia/shared-tuple-call-commit-20261111/shared-view-output-lifetime.log`.
La ampliación aún no demuestra un árbol limpio ni cubre todas las formas de
procedencia de ramas condicionales.

El rastreo se amplió para seguir argumentos de vista en llamadas que devuelven
una vista, incluidas llamadas a métodos, funciones de namespace e indirectas.
La regresión añade una función que devuelve la vista recibida: se prueban
propietarios locales, consumidos y vistas válidas del llamador. La prueba
dirigida pasó 44 análisis (36 rechazos y ocho controles aceptados) y 16
ejecuciones seguras O0/O2. La regla es conservadora y puede mantener una
dependencia aunque el cuerpo no devuelva ese argumento. Una llamada sin
argumentos de vista conserva su procedencia como desconocida. La evidencia
se obtuvo con el compilador de trabajo y no certifica un árbol limpio,
contratos binarios históricos ni todas las formas indirectas de procedencia.
El archivo del analizador preparado para el commit se compiló por separado
con las cabeceras y configuración del árbol de trabajo; no se enlazó ni se
ejecutó un compilador compuesto únicamente por los archivos registrados.
