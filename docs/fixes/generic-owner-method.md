# Método genérico dentro de un tipo genérico

El caso original `A:/nsc-fixes-evidencia/cases/70-method` contiene
`FixedMatrix<T, const int Rows, const int Columns>` con un método
`Multiply<const int OtherColumns>`. El `Main` original solo invoca
`CellCount`, pero la presencia de `Multiply` basta para que
`--check-project-generics` lo rechace. El comando y diagnóstico actuales
están en `generic-owner-method-current.cmd` y `.log` bajo
`A:/nsc-fixes-evidencia`.

Decisión de diseño para este caso: admitir la especialización combinada del
propietario y el método, conservando cada parámetro y rechazando argumentos
incompatibles. Las dos dimensiones de especialización deben formar parte de
la identidad del resultado; esta elección responde a la autorización de
tomar decisiones recomendadas y aún no constituye una implementación.

En un ensayo reversible se retiró solo el diagnóstico explícito de
`collectGenericMethodTemplate` y se compiló incrementalmente el compilador.
El ejemplo original pasó el análisis genérico, lo que no demuestra la
ejecución de su método. Un control independiente
`generic-owner-method-probe/` sí invocó `Box<int>.Echo<int>(42)` y ejecutó 42
en O0. El proyecto `generic-owner-matrix-probe/` conserva una copia sin cambios
de `FixedMatrix.ns` y llama a `Multiply<2>` con datos y resultado comprobable.
La compilación O0 rechazó tres referencias a `Rows` o `Columns` con el
diagnóstico de que se necesita un argumento constante; su `build.log`
conserva los mensajes. El ensayo revela que retirar la prohibición no basta
para el caso original. No se evaluaron O2, invalidaciones de caché,
sobrecargas, bibliotecas binarias ni argumentos inválidos.

Se restauró el diagnóstico y se recompiló incrementalmente. La reproducción
original vuelve a rechazarse, mientras la prueba dirigida de coherencia del
prototipo de Arc sigue pasando. No queda habilitada la capacidad parcial; el
caso continúa pendiente. El build coexistía con cambios sin registrar y no
certifica una revisión limpia. Siguiente paso: rastrear cómo se especializa
la declaración del método y dónde se pierden las sustituciones del dueño,
antes de cambiar la identidad o el planificador de especializaciones.

Un segundo ensayo separó las sustituciones: `Box<int>` y `Box<long>` con un
método que usa el tipo del dueño ejecutaron 42 en O0; dos instancias de un
`Box<const int N>` cuyo método suma N y su propio parámetro constante también
ejecutaron 42. En cambio, el control reducido `Pair<N, M>` como retorno y local
del método reproduce el rechazo de N antes de cerrar el dueño. Los proyectos
y comandos están en `generic-owner-dependent-probe`,
`generic-owner-const-probe` y `generic-owner-nested-const-probe` bajo
`A:/nsc-fixes-evidencia`.

`completeMethodSpecialization` procesa el clon del método inmediatamente,
aun cuando su padre es una plantilla abierta. En un tercer ensayo se retiró
la prohibición y se omitió ese procesamiento cuando el padre conservaba
parámetros genéricos. El control `Pair<N, M>` pasó O0 con tres combinaciones
de argumentos del dueño y método; el proyecto que invoca el `Multiply` del
ejemplo original compiló y ejecutó 42 en O0. El control de dos dueños con
parámetros de tipo también ejecutó 42. Esto apoya la hipótesis de análisis
prematuro, pero aún no demuestra los órdenes de declaración, sobrecargas,
argumentos inválidos, cachés, consumo binario ni O2.

Se retiraron ambos cambios del ensayo y se recompiló incrementalmente. El
rechazo original se restauró y `conditional-unsafe-coherence-after.cmd` pasó.
El ensayo queda pendiente de pruebas y no forma parte del compilador vigente;
su condición era omitir `processNode` en `completeMethodSpecialization` solo
si `state.parent` conserva parámetros genéricos. No se considera un arreglo.

Al repetir el ensayo, `generic-owner-method-regression.cmd` ejecutó la matriz
original, dos propietarios y el retorno `Pair<N, M>` con salida 42 en O0/O2.
También rechazó un tipo como argumento constante y una cantidad incorrecta de
argumentos con diagnósticos específicos. Otro control de tipo inexistente se
rechazó durante parseo, por lo que no acredita la validación semántica de esa
forma. El comando, script y salida están bajo `A:/nsc-fixes-evidencia`.

El control binario `generic-owner-method-binary.cmd` no llegó al consumidor:
el productor rechazó la verificación de metadata con `generic method ... does
not have a materializable class owner 'Fixture.Box'`. El diagnóstico procede
de `src/Driver/PrecompiledSurface/PrecompiledSourceGeneration.cpp`: el
reconstructor busca un tipo propietario no genérico en `metadata.types` y
exige categoría clase. El caso utiliza un struct genérico. Esta ruta es de la
superficie NativeSharp precompilada; no se concluye que corregirla requiera
cambiar interoperabilidad C/C++. Falta demostrar cómo publicar el método sin
duplicarlo respecto de la fuente de su plantilla propietaria, y probar la
reconstrucción y las cachés. Se retiró de nuevo el ensayo del compilador, se
recompiló incrementalmente y pasó el control de coherencia de Arc. El caso
permanece abierto y el resultado binario no se presenta como aprobado.

Un ensayo posterior en el generador de superficie omitió reconstruir un método
por separado si encontraba una plantilla de propietario de igual nombre. Con
un productor que solo contenía `Box<N>`, la biblioteca se emitió y el
consumidor ejecutó 42 en O0/O2. También pasó el control existente de método
genérico privado en una clase no genérica. Sin embargo, al añadir una clase
`Box` no genérica con otro método, el consumidor dejó de encontrar ese método:
la coincidencia por nombre no distingue ambos propietarios. Un ensayo que
exigía encontrar el texto del método dentro de la fuente del tipo hizo pasar
ese ejemplo, pero la comparación textual no acredita identidad ni preserva
necesariamente las referencias reescritas.

Al dar a los dos tipos homónimos métodos genéricos `Identity<U>` idénticos,
el productor fue rechazado antes de emitir metadata: el frontend los agrupa
como implementaciones de una misma familia, con restricciones equivalentes.
La última ejecución de `generic-owner-method-binary.cmd` conserva ese rechazo
en su `.log`; las ejecuciones positivas anteriores sobrescribieron el mismo
log, de modo que sus salidas constan solo en el registro de la sesión y no en
archivos independientes. La fuente actual del fixture conserva el caso que
falla. Es necesario distinguir la identidad del dueño genérico de la del dueño
no genérico tanto al recoger métodos como al reconstruir la superficie; no se
considera segura una coincidencia por nombre o texto.

Se retiraron los tres cambios experimentales del frontend y del generador, se
recompiló incrementalmente y pasó de nuevo
`conditional-unsafe-coherence-after.cmd`. La implementación sigue pendiente;
no se modificaron los fuentes de interoperabilidad C/C++ ni el formato de
metadata.

La investigación de la colisión muestra dos fronteras distintas. En
`collectGenericMethodTemplate`, la clave canónica y la clave de familia usan
el nombre escrito del dueño sin su aridad; por ello un método de `Box<N>` y
uno de `Box` se agrupan. En un ensayo se añadió la aridad del padre genérico
a esa clave y se volvió a habilitar provisionalmente la especialización. El
productor con métodos `Identity<U>` en ambos tipos pasó el error previo de
familias, pero el consumidor binario rechazó las llamadas por no encontrar
plantillas visibles (`generic-owner-method-binary.log`). Un control reducido
en un solo proyecto también rechazó ambas llamadas
(`generic-owner-collision-probe/build.log`). Los controles anteriores de la
matriz y tipos distintos siguieron pasando O0/O2.

La resolución actual en `GenericCallInference.cpp` parte de la escritura del
receptor, y `TemplateResolver.cpp` agrupa candidatos por nombre de dueño y
módulo sin incluir su aridad. La prueba demuestra que cambiar solamente la
clave de familia no resuelve la llamada; es necesario transmitir y verificar
la identidad del tipo del receptor también en esa selección, en la caché y en
la superficie binaria. No se ha demostrado todavía la solución. Se retiró el
ensayo, se recompiló incrementalmente y el control de coherencia de Arc pasó.

Un ensayo adicional consultó `inferExpressionType` para el receptor de una
llamada local y comparó nombre y aridad de los padres candidatos del mismo
proyecto. Junto con la separación provisional de familias, el control
`generic-owner-collision-probe` compiló y ejecutó 42 en O0 con `Box<20>` y
`Box` no genérico, cuyos métodos producen resultados distintos. La matriz
dirigida anterior volvió a pasar O0/O2. Esta selección era solo una prueba:
nombre, aridad y proyecto no demuestran identidad entre namespaces.

El control `generic-owner-namespace-probe` llamó `Identity<int>` en
`Beta.Box<20>`, que no tiene ese método, mientras `Alpha.Box<N>` sí lo tiene.
La compilación se rechazó en el análisis posterior por receptor incompatible,
pero el diagnóstico ya nombraba el método materializado con nombre
especializado. Esa evidencia muestra que el ensayo alcanzó una plantilla
homónima antes del rechazo; no demuestra una ejecución incorrecta. Se necesitan
identidades resueltas del receptor y del dueño, no esa comparación textual.
Comandos y logs de los dos controles están en `A:/nsc-fixes-evidencia`. Se
retiró el ensayo, se recompiló incrementalmente y el control de coherencia de
Arc volvió a pasar. El soporte de método genérico sigue deshabilitado.

En otro ensayo se usó el tipo inferido para buscar la declaración propietaria
exacta en los modelos existentes. El control de `Box` y `Box<N>` del mismo
namespace ejecutó 42; `Beta.Box<N>` sin método se rechazó en la selección de
plantilla, sin materializar el método de `Alpha.Box<N>`. Un positivo con ambos
tipos genéricos totalmente calificados y métodos distintos ejecutó 42 en O0.
Al reemplazar `Alpha.Box<20>` por `Box<20>` con `using Alpha`, el análisis
rechazó el tipo como ambiguo. Comandos y fuentes están en
`generic-owner-collision-probe`, `generic-owner-namespace-probe` y
`generic-owner-namespace-positive` bajo `A:/nsc-fixes-evidencia`; el último
fixture conserva la versión con import y su log fallido. No se preservó en un
log separado la salida de ejecución positiva con nombres calificados.

Un control independiente, `generic-import-baseline`, quitó los métodos
genéricos y mantuvo `Alpha.Box<N>`, `Beta.Box<N>` y `using Alpha`. El
compilador incremental restaurado sigue rechazando `Box<42>` como ambiguo. Esto
muestra que el problema de import no fue introducido por el ensayo de métodos,
pero no es una comparación con la revisión base del repositorio. El log y el
comando están en esa carpeta de evidencia. El ensayo de identidad del receptor
se retiró; `conditional-unsafe-coherence-after.cmd` volvió a pasar. Falta
investigar el ámbito de imports y la publicación binaria antes de habilitar el
caso sin perder esos usos.

El contraste `generic-import-order.cmd` usa únicamente tipos genéricos con
métodos no genéricos. Con dos `Box<N>` en namespaces distintos, el nombre
totalmente calificado y el namespace léxico ejecutaron 42 en O0; un solo
`using Alpha` se rechazó como ambiguo, como también ocurrió sin imports o con
ambos imports. Con un único `Alpha.Box<N>` y sin import, `Box<42>` se aceptó
y ejecutó 42. La expectativa de la prueba era rechazar ese último uso por
falta de visibilidad, pero se registra como comportamiento observado, sujeto a
contraste con el contrato de resolución. El script es diagnóstico y termina
con código 0 aunque detecte diferencias; no debe contarse como regresión
aprobada. La salida completa está en `generic-import-order.log` en A.

`resolveTypeTemplate` hace fallback entre los candidatos visibles por nombre
terminal y no recibe el ámbito de imports, mientras la selección de métodos
usa una escala de nombres. Esta diferencia explica las observaciones de este
fixture, no demuestra por sí sola cuál es la modificación segura para todos
los consumidores. No se ha cambiado esa función compartida ni se ha
considerado resuelto el soporte de métodos genéricos.

La comprobación más reciente del ejemplo original volvió a reproducir el
rechazo (`generic-matrix-method-current.log`). Con la prohibición retirada solo
durante un ensayo, el ejemplo sin llamada compiló; una copia que invoca
`Multiply<2>` rechazó las dimensiones `Rows` y `Columns` en O0 y O2
(`generic-method-owner-probe.log`). Dos controles sencillos con propietarios
de tipo y constante distintos devolvieron 42 en O0 y O2
(`generic-method-owner-type-probe.log`); no demuestran identidad correcta en
todas las rutas. Se restituyó la prohibición, el archivo quedó sin diff y el
compilador se recompiló incrementalmente (`generic-method-owner-restore.log`
y `generic-method-owner-restore-build.log`).

El contraste `generic-import-nongeneric-probe.cmd` examinó nombres sin
calificar desde el mismo ámbito. Para un tipo ordinario, un nombre externo sin
import se rechaza y un `using Alpha` selecciona `Alpha.Box` incluso si existe
`Beta.Box`. Para la plantilla equivalente, el caso único sin import se acepta
y el caso con ambos namespaces y solo `using Alpha` se rechaza por ambiguo.
Los seis comandos y diagnósticos están en `generic-import-nongeneric-probe.log`.
Esto demuestra una diferencia de visibilidad en esta reproducción; el script es
diagnóstico, no una prueba automática aprobada. El resolvedor genérico comparte
una escala de nombres con el enlazado ordinario para métodos, pero la ruta de
tipos aún usa el fallback por nombre terminal. La siguiente acción es diseñar
la resolución de plantillas de tipo con el contexto léxico real en sus puntos
de demanda, conservando alias, namespaces externos, visibilidad por proyecto
y comportamiento de caché; no basta insertar una comparación por nombre. No
se ha habilitado la capacidad ni cambiado ese resolvedor.

La regresión provisional `tests/integration/generic_type_lexical_resolution.py`
fija los seis controles de visibilidad y, para los positivos, compila y
ejecuta en O0/O2. Con el compilador restaurado fallan cuatro comprobaciones:
el genérico sin import se acepta en ambos modos y el caso con un único import
entre dos namespaces se rechaza en ambos. Los controles ordinarios y el
genérico con un solo tipo e import pasan; detalles en
`A:/nsc-fixes-evidencia/generic-type-visibility-before.log`. La prueba queda sin
registrar mientras esté fallando. No se cambió la expectativa ni el ejemplo del
reporte para obtener un resultado favorable.

La ampliación `generic-type-visibility-expanded.cmd` añade dos imports y un
alias de namespace a la comparación, en O0/O2. Con dos imports se rechazaron
por ambigüedad tanto el tipo ordinario como el genérico. Con
`using Select = Alpha`, `Select.Box` ordinario ejecutó 42 en ambos modos,
pero `Select.Box<int>` se rechazó como plantilla no visible en ambos. Se
mantienen los cuatro fallos anteriores: aceptación sin import y rechazo con
un import entre dos namespaces. El resultado total es 14/20 comprobaciones
según las expectativas de esta prueba; seis fallaron, incluyendo los dos
nuevos controles de alias. Comando y salida: `A:/nsc-fixes-evidencia/`
`generic-type-visibility-expanded.cmd` y `.log`. El compilador tiene hash de
build `db3b19d61487b862`; coexiste con cambios locales y no certifica HEAD.

El examen de las cuatro rutas que invocan `resolveTypeTemplate` muestra que
`processTypeSyntax`, `collectTypeDependency` y `processNodePrimaryTypeSyntax`
ya reciben `GenericLookupScope`; `BodyTypeSpecializationDemand` también guarda
una copia de ese ámbito, aunque su resolución no la usa. Por tanto no es
necesario inventar imports a partir del consumidor diferido. Sigue por
demostrar cómo conservar el ámbito de origen en clones y bibliotecas y cómo
tratar la búsqueda explícita `module::type`, que sigue una rama separada. No
se ha modificado el resolvedor compartido ni se ha demostrado que corregirlo
sea indispensable para el ejemplo original de matriz, que no usa imports.

Se ensayó pasar `GenericLookupScope` a las cuatro rutas de resolución y usar
`qualifiedNameLadder`. El compilador incremental terminó correctamente; el
fixture de visibilidad pasó sus 20 comprobaciones O0/O2
(`generic-type-visibility-expanded.log`). También pasaron el control de caché
de imports y las tres pruebas binarias seleccionadas (`generic-type-lexical-
cache.log` y `generic-type-lexical-binary.log`). Sin embargo, la prueba de
punteros con Core binario falló al compilar por tipos internos como
`SharedLocalHandle`, `SharedAtomicHandle` y `SharedBorrow`
(`generic-type-lexical-pointers.log`). En las fuentes originales de `Rc` esos
tipos dependen de `using NativeSharp.Core.Internal`; no se ha demostrado por qué
ese ámbito no llegó al punto que falló en el consumidor precompilado. No se
considera correcto recurrir de nuevo al nombre terminal sin preservar y probar
el origen de la declaración.

Se conservó el diferencial de ensayo en
`A:/nsc-fixes-evidencia/generic-type-lexical-prototype.patch` y se retiró del
árbol después de comprobar que esos cinco archivos no tenían cambios previos;
no queda diff de ellos. La compilación incremental restaurada terminó con
código 0 y `nsc.exe` SHA-256
`44aad30ce47dcdc3450f2b0ab8d1bc94352b89fbb17d79ec668ea7ec7677c662`.
La regresión de punteros binarios volvió a pasar en O0/O2, incluidas las
fábricas y controles de solo lectura (`generic-type-lexical-restored.cmd` y
`.log`). Usa el Core binario preexistente, no uno regenerado. El archivo de log
de build se reutilizó durante la restauración y ya no conserva la primera
compilación. El defecto de visibilidad y el método genérico siguen abiertos;
la próxima investigación debe conservar el ámbito del productor sin hacer
visibles tipos internos al consumidor.

Un segundo ensayo conserva el ámbito de declaración al procesar el clon de
una plantilla de tipo. `SpecializationExecutor.cpp` usaba un ámbito vacío en
la ruta de especialización fría y en dos caminos de restauración de caché; ahora
recupera los imports y alias del registro, igual que la especialización de
métodos. El resolvedor usa la escala de nombres con el ámbito disponible en sus
cuatro llamadores. Este cambio permanece provisional y sin commit.

Con el compilador aislado SHA-256
`0cae8367d958b10941be63154972d282cf021b1ab0778624d6e3f4251a7ec361`,
`generic-type-visibility-expanded.cmd` pasó los 20 controles O0/O2. La nueva
prueba `generic_type_declaration_import_cache.py` usa aliases distintos en
declaración y consumidor: obtuvo 21 y 33 según el import de la declaración,
en frío y en caliente, e informó aciertos de `compilation-bundle-cache` en las
dos repeticiones. La selección de caché de declaraciones pasó 1/1, los tres
controles binarios pasaron 3/3 y el control de punteros con Core binario
preexistente terminó con código 0 (`generic-type-lexical-controls.cmd` y los
logs `generic-type-lexical-{cache,binary,pointers}.log`). También pasó el
control dirigido de coherencia de Arc. La compilación fue incremental.

El acierto de bundle no demuestra por sí solo que las dos rutas específicas
de restauración C3 hayan sido ejecutadas; tampoco se ha verificado todo el
comportamiento de imports de declaraciones sustituidas, módulos explícitos o
consumidores externos. El fixture original de matriz con método genérico
permanece rechazado: este ensayo corrige una reproducción de visibilidad, no
habilita ni cierra el caso del informe. No se regeneró Core y el árbol contiene
otros cambios locales.

La prueba de caché de tipos se amplió a O0/O2 y a cambiar únicamente el archivo
del consumidor tras una ejecución caliente. En las cuatro combinaciones de
import del productor y optimización, el cambio produjo `compilation-bundle-cache:
not-found` y ejecutó el valor esperado (21 para Alpha y 33 para Beta). El bundle
caliente anterior sí informó `hit`. Esto comprueba una nueva compilación sin
reutilizar el bundle completo, pero aún no identifica un hit específico C3.

La nueva prueba `generic_type_declaration_import_binary.py` compila una
biblioteca con `Pick = Alpha` y un consumidor con `Pick = Beta`: los cuatro
cruces productor/consumidor O0/O2 devolvieron 21, el valor del productor.
Se registraron ambas pruebas y la matriz léxica en CTest; la selección dirigida
pasó 3/3 (`generic-type-lexical-ctest.cmd` y `.log`). El análisis del ejemplo
original `70-method` se repitió y sigue rechazando el método genérico del dueño;
la corrección de visibilidad no cierra ese caso. Los cambios siguen sin commit.

El nuevo control `generic_type_lexical_projects.py` usa un `ProjectReference`
real: el consumidor rechazó en O0/O2 el genérico sin import y el tipo interno;
el tipo público importado, totalmente calificado o mediante alias ejecutó 42.
Se comprueban los diagnósticos, no solo que la compilación falle. La selección
dirigida de CTest pasó ahora 4/4 (`generic-type-lexical-ctest.log`); esto no
acredita todas las reglas Friend ni todas las rutas de módulos precompilados.

Se intentó inspeccionar la caché C3 mediante `--dump-project-generics` con
`--artifact-cache`, pero el CLI rechaza esa combinación antes de compilar
(`generic-type-dump-cache-option-rejected.log`). Se retiró esa invocación del
fixture y se repitió la selección 4/4. La cobertura de las rutas específicas
de restauración C3 permanece no demostrada; los aciertos de bundle y el caso de
bundle invalidado no deben presentarse como prueba de esas rutas.

Una prueba directa con dos `ProjectCompilation` y una raíz de caché explícita
no encontró el payload de `Wrapper<int>` ni un aumento del contador de aciertos
en la segunda compilación (`generic-type-c3-disabled-probe.log`). La lectura de
`ProjectCompilation::compileProjectGenerics` explica el resultado: el modelo
genérico se crea sin conectar esa raíz a sus cachés C1 y C3; el propio código
registra problemas de corrección por los cuales permanecen desconectadas.
La prueba inicial suponía erróneamente que se conectaban y se retiró sin dejar
diff en el archivo unitario. La prueba existente se reconstruyó
incrementalmente y volvió a pasar sus 92 comprobaciones
(`generic-type-c3-unit.cmd` y `.log`). No se habilitaron las cachés internas ni
se afirma haber probado la restauración C3; las ramas modificadas para cuando
se habiliten siguen sin ejecutar en la configuración actual.

Con la corrección provisional de imports se repitió el ensayo de permitir el
método de un dueño genérico y aplazar el análisis de su clon hasta cerrar el
dueño. La matriz invocada, dos dueños y el retorno con constantes ejecutaron
42 en O0/O2; los argumentos constante/tipo y la aridad incorrectos se
rechazaron (`generic-owner-method-regression.log`). El caso de tipo inexistente
se rechaza en el parser, de modo que no acredita su validación semántica.

Al separar la aridad del dueño en la clave del método, el productor que declara
`Box<N>` y `Box` con `Identity<U>` dejó de colisionar y emitió la biblioteca.
Su consumidor, sin embargo, no encontró `Make` ni `Identity` sobre los
receptores (`generic-owner-method-binary.log`). La identidad de la familia no
basta: la selección de una llamada sobre un valor debe vincularse al tipo
resuelto de ese receptor, también entre módulos. No se consideran implementados
esos métodos por los ejemplos positivos de una sola familia.

Se retiraron solo los cambios del ensayo de métodos, conservando el ensayo de
imports; `TemplateCollector.cpp` no tiene diff. El build incremental restaurado
terminó con código 0 y el compilador tiene SHA-256
`9b5509e0c0e7f8651c05140e4ab6e7dddd269ec55a9bceba961bfe755cf394de`.
La selección de imports pasó 4/4 y el control de coherencia de Arc volvió a
pasar. La reproducción original vuelve a rechazar la declaración. Los logs
de build se reutilizaron en cada ensayo; el de regresión binaria conserva el
último fallo, no las salidas anteriores. El caso 70 sigue abierto.

Un ensayo posterior, aún sin commit, diferencia la clave del método mediante
la aridad del dueño genérico y evita reconstruir por separado el método cuando
la identidad canónica exacta del dueño ya publicó su fuente de plantilla. En la
selección de llamadas explícitas se usa, cuando está disponible, la declaración
inferida del receptor: para tipos genéricos se compara la declaración de la
plantilla, y para tipos ordinarios se contrasta proyecto y nombre declarado.
Se conserva el diagnóstico de aridad incorrecta cuando hay un único candidato
del mismo dueño. La instrumentación temporal se retiró; el ensayo permanece en
el árbol para continuar su validación.

Con compilación incremental y `nsc.exe` SHA-256
`15b2f16b79a609510f5daafe3f5f5bd0354ed286415c46d3dc1e3c1f7ef261dd`, la
matriz invocada y los controles de dueños múltiples devolvieron 42 en O0/O2;
el productor binario con `Box<N>` y `Box` homónimos y su consumidor devolvieron
42 en O0/O2 (`generic-owner-method-{regression,binary}.log`). La llamada a
`Beta.Box<20>.Identity<int>` cuando solo `Alpha.Box` tiene el método se rechazó
en la selección (`generic-owner-namespace-next.log`). La selección léxica pasó
4/4 y los controles binarios de genéricos, caché de bundle y punteros pasaron
antes del último ajuste acotado al diagnóstico de aridad
(`generic-type-lexical-{ctest,cache,binary,pointers}.log`).

Esto no cierra el caso: falta comprobar inferencia implícita, receptores cuya
procedencia no se puede inferir o ya están reescritos, overloads y restricciones,
otros namespaces/imports, DLL y cachés. La prueba unitaria histórica todavía
espera el rechazo de esta capacidad y debe actualizarse solo después de fijar
sus criterios de aceptación. El compilador se construyó con otros cambios
locales; no certifica una revisión limpia.

La regresión `generic_owner_method_inference.py` mostró que, con dos dueños
homónimos en namespaces distintos, las llamadas explícitas ejecutaban 42 pero
las implícitas no encontraban `Echo`; ambos niveles fallaban antes del ajuste.
La inferencia ahora filtra candidatos por la declaración del receptor cuando
este es un identificador local cuyo tipo puede resolverse. Tras la compilación
incremental, llamadas explícitas e inferidas ejecutaron 42 en O0/O2 para ambos
dueños; los receptores de un tercer namespace sin el método se rechazaron con
los diagnósticos esperados (`generic-owner-method-inference-ctest.log`).
La prueba registrada pasó 1/1; la matriz y el consumo binario dirigido también
pasaron (`generic-owner-method-{regression,binary}.log`).

Se reemplazó la expectativa unitaria que exigía rechazar toda declaración de
método genérico en dueño genérico por un control positivo de declaración; la
prueba dirigida de monomorfización pasó (`generic-type-c3-unit.log`). El
compilador del ensayo tiene SHA-256
`d579c92b1568a7be9fed2b4811b320e28fcef8b762798e29d35985735bed28c0`.
El seguimiento de inferencia está probado solo para receptores identificadores;
faltan receptores calculados, overloads, restricciones, DLL, caché y pruebas
de conflictos entre módulos. El cambio sigue sin commit y no cierra el caso.

La regresión de inferencia incorpora ahora dos sobrecargas `Echo<U>`, una para
un valor y otra para un arreglo. Las formas explícita e inferida eligieron la
sobrecarga esperada en O0/O2, manteniendo los dos dueños homónimos y los
rechazos del dueño sin método. La nueva `generic_owner_method_binary.py`
construyó productores estáticos y DLL en O0/O2 y consumidores en O0/O2; las
ocho ejecuciones devolvieron 42 con métodos de dueño genérico, dueño ordinario
homónimo, retorno dependiente y las dos sobrecargas. Ambas pruebas registradas
pasaron 2/2 (`generic-owner-method-directed-ctest.cmd` y `.log`).

Estos fixtures cubren esas sobrecargas y cruces binarios, no todas las
combinaciones de overloads, restricciones, carga o descarga de DLL. Persisten
los límites sobre receptores calculados, identidad en casos no inferibles y
otras rutas de procedencia; el cambio sigue provisional y sin commit.

El control posterior añadió receptores calculados con retorno conocido y
restricciones `where U : class`: los casos explícitos e inferidos válidos
ejecutaron 42 en O0/O2 y los controles inválidos se rechazaron. Además,
`Holder<T>.Keep<TOp> where TOp : Reducer<T>` reveló que se perdía el argumento
del método al clonar la especialización dentro del dueño cerrado. Se conserva
ese argumento en el clon ya especializado y se comprueba la cláusula en el
miembro cerrado. `Holder<int>` con `Reducer<int>` ejecutó 42, mientras
`Holder<long>` con la misma implementación se rechazó; la prueba que usa
ambos dueños en el mismo programa también rechazó el segundo.

La inferencia de `Keep(42, op)` fallaba porque se comparaba el parámetro abierto
del dueño con el tipo concreto. El ensayo sustituye los parámetros del dueño
conocidos por el receptor al evaluar el candidato. Las formas inferidas válida
e inválida pasaron sus expectativas en O0/O2, usando un valor `long` para el
dueño `Holder<long>`. La selección dirigida registrada, que incluye consumo
estático y DLL, pasó 2/2; `nsc.generics.dependent-constraints` pasó 1/1 con
el entorno aislado (`A:/nsc-fixes-evidencia/generic-owner-method-directed-ctest.log`
y `generic-owner-dependent-ctest.log`). Las compilaciones fueron incrementales.

Una primera invocación del control existente omitió las variables del entorno:
el test usó temporalmente `C:/Windows/Temp` y falló al buscar un runtime de
instalación inexistente. Se repitió mediante
`A:/nsc-fixes-evidencia/generic-owner-dependent-ctest.cmd`, que fija temporales
y runtime en A; esta repetición es la que pasó. No se modificó ni desplegó en
`C:/native-sharp`.

El cambio permanece sin commit y el caso sigue abierto: faltan controles de
restricciones dependientes a través de bibliotecas, receptores cuya identidad no
puede inferirse y regresiones adicionales de selección e inferencia. El árbol
contiene otros cambios locales; estos resultados no certifican una revisión limpia.

El contraste binario se amplió con `Holder<T>.Keep<TOp> where TOp : Reducer<T>`.
Cuando el consumidor declara `IntReducer` e implementa allí `Reducer<int>`,
los cruces de biblioteca estática y DLL con productor y consumidor O0/O2
ejecutan 42 para el caso válido y rechazan el inválido; la selección dirigida
pasó 2/2 (`generic-owner-method-directed-ctest.log`).

Cuando `IntReducer` y su `impl Reducer<int>` pertenecen al productor, los ocho
consumidores válidos se rechazan indicando que la estructura no implementa el
trait; el negativo también se rechaza. Evidencia:
`A:/nsc-fixes-evidencia/generic-owner-binary-trait-producer-failure.log`. Un
control independiente con `Check<U>` en una clase no genérica reproduce el
mismo rechazo en los ocho cruces
(`generic-owner-producer-trait-control.cmd` y `.log`), por lo que no es
exclusivo de dueños genéricos. El volcado de la biblioteca sí registra a
`IntReducer` con la interfaz especializada `Reducer<int>`
(`generic-owner-trait-abi.log`). `emitNominalBases` omite interfaces de structs
ordinarios al reconstruir su declaración; esta es una frontera candidata, no
una causa ya demostrada. La reproducción conservada está en
`A:/nsc-fixes-evidencia/temp/nsc-owner-trait-inspect-ardvlnh0`.

Queda pendiente rastrear la reconstrucción y comprobar una solución sin perder
otras relaciones ni alterar la interoperabilidad C/C++. El caso binario no se
considera cerrado: el fixture registrado cubre por ahora la implementación en
el consumidor y el log separado conserva el fallo del productor.

La inspección confirmó que `PrecompiledSourceGeneration` utiliza la superficie
de `PublicSurfaceBuilder`, cuyo `emitNominalBases` omitía para structs ordinarios
las interfaces ya presentes en metadata. El control aislado
`trait-struct-syntax/probe.cmd` comprobó que la sintaxis directa de struct con
trait compila y ejecuta 42. Se ajustó la reconstrucción para emitir la lista de
interfaces de esos structs, sin cambiar el formato de metadata. El control de
método en clase ordinaria, antes fallido, pasó los ocho cruces binarios
(`generic-owner-producer-trait-control.log`).

La regresión registrada `generic_owner_method_producer_trait.py` conserva el
trait y su implementación en el productor: en biblioteca estática y DLL, con
productor y consumidor O0/O2, la llamada inferida al método del dueño genérico
invoca `Combine` y devuelve 42; el argumento que no implementa el trait para el
tipo del dueño se rechaza. La selección de tres pruebas pasó 3/3
(`generic-owner-method-directed-ctest.log`). También terminaron con código 0 los
controles existentes de caché de imports, binarios genéricos y punteros con Core
binario preexistente (`generic-type-lexical-controls.cmd` y logs asociados).
No se regeneró ese Core, y los resultados no demuestran todos los usos de
interfaces ni de receptores. El caso 70 y los cambios globales siguen abiertos.
Después del ajuste también terminaron con código 0 la matriz del ejemplo
original y sus controles (`generic-owner-method-regression.cmd`) y los cuatro
controles de visibilidad de tipos (`generic-type-lexical-ctest.log`).

La selección dirigida añade llamadas estáticas explícitas e inferidas sobre
`Alpha.Box<int>` y una llamada estática inferida sobre `Box<20>` en los dos
fixtures binarios. Las tres pruebas volvieron a pasar 3/3 en la configuración
dirigida; las bibliotecas estáticas y DLL se cruzaron entre O0 y O2. Esto cubre
esas llamadas concretas, no todos los receptores estáticos posibles.

Un control posterior añade llamadas desde un miembro del mismo dueño:
`Own<T>.Run()` llama a `Identity<int>(42)`, con y sin `this`, y también a
`Identity(42)` inferido. Las cuatro formas se rechazan en O0/O2 porque el
miembro especializado no se encuentra; la forma explícita sí ejecuta 42 con
un dueño `Own` no genérico. La prueba `generic_owner_method_inference.py`
queda fallando intencionalmente para conservar las reproducciones y el log es
`A:/nsc-fixes-evidencia/generic-owner-method-inference.log`.

El ejemplo mínimo independiente está en
`A:/nsc-fixes-evidencia/generic-owner-internal/`. Su dump registra primero el
tipo cerrado `Own<int>` y después el método `Identity<int>`. El productor del
tipo clona el árbol antes de procesar las llamadas de sus cuerpos, mientras la
especialización del método se agrega al padre de la plantilla abierta. Esto es
coherente con la ausencia observada en el dueño ya clonado, pero aún debe
contrastarse con la propagación de miembros, sobrecargas y caché antes de fijar
una corrección. No se considera cerrado el caso ni aprobada la selección de
regresiones mientras esta prueba falle.

Un ensayo sin commit propaga las especializaciones tardías desde el método
genérico correspondiente del dueño ya cerrado, identificado por su plantilla y
posición de origen. Con ese ensayo, las llamadas internas explícitas ejecutan
42; un ajuste adicional de selección permite las inferidas cuando los patrones
no dependen del dueño. Pasaron también un control de dos dueños cerrados con
llamada explícita, uno de sobrecargas y uno de recursión en O0/O2. El resultado
intermedio de los tres tests dirigidos fue 3/3 antes de añadir el último control.

El control nuevo `own-two-types-inferred` hace que el método tome un `T` del
dueño y deduzca su otro parámetro genérico. Se rechaza en O0/O2 porque la
inferencia compara todavía el patrón abierto con el argumento del clon. El
log `A:/nsc-fixes-evidencia/generic-owner-internal-dependent-failure.log`
conserva el resultado. Falta usar las sustituciones del dueño cerrado también
en esa inferencia, comprobar la propagación cuando una especialización ya existe
y revisar errores de clonación, constraints y cachés antes de conservar el
ensayo. La prueba dirigida vuelve a estar fallando; no se declara resuelto.

El ensayo posterior usa para inferencia el miembro genérico ya sustituido del
dueño cerrado, seleccionado por plantilla y posición de origen. El control de
dos dueños con `T` en el parámetro y el retorno ejecuta 42 en O0/O2, con y sin
`this`; también pasó una demanda compartida por una llamada interna y otra
externa. La selección de tres pruebas dirigidas volvió a pasar 3/3 e incluye
ahora llamadas internas del productor en bibliotecas estáticas y DLL. El
control existente de restricciones dependientes pasó 1/1 y la matriz del
ejemplo original volvió a terminar con código 0.

El código sigue siendo provisional y sin commit: la propagación y la búsqueda
del patrón cerrado todavía necesitan revisión de errores de clonación, packs,
constraints no seleccionados, orden y caché. Estas pruebas usan el árbol con
otros cambios locales y no demuestran todos los usos del lenguaje.

Los controles posteriores pasaron con un segundo dueño cuyo método no se usa,
y con una llamada interna a un método con pack constante tanto válida como con
índice fijo fuera de rango. Sin embargo, si el índice de `packat(Ns, N)` es un
parámetro constante del dueño, `Own<1>` y `Own<2>` se rechazan ambos antes de
sustituir N: `foldValuePackAt` intenta evaluar el índice al clonar el método en
la plantilla abierta. El caso válido debería ejecutar 42 y el inválido debería
diagnosticar el índice fuera de rango una vez cerrado el dueño. La prueba
dirigida conserva ambos casos y falla; la salida está en
`A:/nsc-fixes-evidencia/generic-owner-dependent-pack-failure.log`.

Resolverlo exige conservar la operación y los argumentos del pack hasta conocer
la sustitución del dueño, o materializar desde la declaración cerrada sin perder
demandas posteriores. Todavía no se ha demostrado una solución segura para
ambos órdenes ni para errores y cachés. El ensayo de propagación sigue sin
commit y no debe tratarse como soporte completo de métodos genéricos anidados.

Se ensayó representar el `packat` pendiente con sus valores hasta cerrar el
dueño. Los ejemplos `Own<1>` y `Own<2>` pasaron sus expectativas de ejecución y
diagnóstico, pero un tercer ejemplo que solo llama a `Own<1>.At` y además
declara un `Own<2>` sin usar el método falló: la propagación provisional
materializa el método en ambos dueños y diagnostica el índice del segundo.
Esto identifica un problema de demanda, no solo de evaluación diferida. La
evidencia está en `generic-owner-deferred-pack-unused-failure.log` y el diff
experimental en `generic-owner-deferred-pack-prototype.patch`, bajo
`A:/nsc-fixes-evidencia`.

Se retiró la parte experimental de evaluación diferida y se recompiló
incrementalmente. El ensayo previo de propagación permanece; la regresión
actual falla las tres variantes de índice dependiente en O0/O2
(`generic-owner-method-inference.log`). Siguiente paso: asociar cada demanda de
método con el dueño cerrado correspondiente y preservar las demandas para
dueños creados después; un método no llamado no debe producir ese error.

El control `generic-owner-demand-order` pide `Identity<int>` sobre `Own<1>` y
`Own<2>`. El dump muestra primero una única especialización del método y
después los dos dueños, con un hit de la clave del método. Por ello, asociar un
puntero al dueño que ya esté materializado no basta: a veces todavía no existe.
Se añadió `own-dependent-pack-two-demands`, que requiere ambos dueños con índices
válidos distintos; también falla en O0/O2. Fuentes, comando y dump están en
`A:/nsc-fixes-evidencia/generic-owner-demand-order/`.

La siguiente implementación debe identificar la instancia del dueño desde la
escritura resuelta del receptor antes de compartir la clave del método, y
coordinar esa demanda con la creación del tipo. Debe mantener distintas las
parejas de argumentos de dueño y método, soportar el orden inverso y detectar
índices inválidos solo cuando se demanda esa pareja. No se ha implementado aún
esa coordinación ni se considera correcto el prototipo de propagación global.

Un nuevo ensayo materializa el receptor cerrado cuando se puede identificar y
usa la pareja de clave de dueño y argumentos del método para especializar el
miembro ya sustituido de ese dueño. Las cuatro reproducciones de packs
dependientes pasan en O0/O2: índice válido, diagnóstico de índice inválido,
dueño inválido cuyo método no se usa y dos dueños válidos con índices distintos.
También pasó una variante estática tras usar el tipo sintáctico del receptor.

Un control adicional con `Own<Item>` mostró dos identidades del mismo tipo:
la copia del receptor no había resuelto su argumento nominal antes de crear la
clave. Se procesan los argumentos de la copia antes de materializar; el dump
posterior registra un único `Own<Item>` y el retorno encadenado ejecuta 42 en
O0/O2 (`generic-owner-return-chain/dump.log`). La selección dirigida pasó 3/3,
incluidos productores estáticos y DLL (`generic-owner-method-directed-ctest.log`).

El cambio sigue provisional: aún es necesario revisar receptores cuyo tipo no
puede identificarse en esta fase, órdenes y cachés, estado de errores anidados y
la interacción con el prototipo anterior de propagación. No se ha hecho commit ni
se considera cerrado el caso con estas pruebas parciales.

Se probaron receptores calculados `Make().At<...>()`: el índice válido ejecuta
42 y el inválido se rechaza con el diagnóstico esperado en O0/O2. En cambio,
`w.value.At<...>()`, donde `value` es un campo de otro objeto, se rechaza antes
de cerrar el dueño. Se reproduce tanto cuando las declaraciones comparten
namespace como cuando el campo pertenece a un tipo de otro namespace.

Un ensayo limitado que buscaba el campo directo hizo pasar el primer caso, pero
no el segundo y no conservaba los imports y alias del archivo que declara el
campo. Se retiró para evitar una selección incorrecta; la compilación
incremental posterior terminó con código 0 y la regresión conserva ambas
variantes fallando en O0/O2 (`generic-owner-method-inference.log`). Es necesario
resolver el campo usando su declaración y su contexto léxico, incluso entre
archivos o bibliotecas; no basta buscar un nombre en el contexto del consumidor.

Un nuevo ensayo conserva por campo la unidad, namespace, imports y alias del
recorrido léxico previo. Con fuentes, los campos directo y de otro namespace
ejecutan 42 en O0/O2; también pasa un control donde declaración y consumidor usan
el mismo alias para namespaces distintos. El ensayo sigue sin commit.

La variante de biblioteca añadida a `generic_owner_method_binary.py` falla en
los ocho cruces de productor y consumidor O0/O2, tanto estáticos como DLL, con
`At` no visible (`generic-owner-field-binary-failure.log`). La metadata publica
la plantilla y la especialización del tipo del campo; el dump del consumidor
registra la especialización importada con identidad, pero sin nodo materializado
(`generic-owner-field-binary-generics.log`). Falta reconstruir la demanda del
método para esa instancia importada sin confundirla con otra ni asumir que el
nodo del dueño ya existe. La selección dirigida vuelve a fallar y el caso sigue
abierto; campos genéricos, heredados y todos los ámbitos binarios siguen sin
verificar.

El control `generic_owner_field_split.py` sitúa los tipos homónimos, el campo y
el consumidor en tres archivos distintos, con alias opuestos en productor y
consumidor. Ejecutó 42 en O0/O2 y quedó registrado en CTest; la prueba dirigida
pasó 1/1 (`generic-owner-field-split-ctest.log`).

Para el caso binario, el dump confirma que `PackOwner<1>` tiene una semilla de
especialización con nombre físico y argumentos, pero `specializedSyntax` es
nulo; la plantilla y su método sí se publicaron. El campo reconstruido se escribe
con el nombre físico porque existe un registro de tipo concreto. El conversor de
metadata ya posee una ruta para escribir especializaciones como tipos genéricos,
pero se usa cuando no existe ese registro y debe contrastarse su tratamiento de
argumentos constantes y anidados antes de reutilizarla. No se ha modificado esa
ruta ni se ha afirmado que cambiar solo la escritura del campo sea suficiente:
la instancia importada también necesita un destino válido para el método.

Un ensayo posterior vincula la semilla importada con su declaración física por
identidad completa y clona localmente el método con los argumentos registrados.
La clasificación del miembro distingue el cuerpo especializado local de una
declaración realmente precompilada. Con argumentos constantes, básicos,
nominales y un `Pair<20,22>` anidado, los cinco controles dirigidos pasaron 5/5.
Se añadió después `Nest<Value>` como argumento anidado: la selección vuelve a
fallar en los ocho cruces binarios porque el ensayo todavía no reconstruye esa
identidad; evidencia en `generic-owner-nested-nominal-failure.log`. El ensayo
permanece sin commit y no cubre todos los formatos de argumentos ni su caché.

Se añadió un segundo control con `ProjectReference` real: productor y consumidor
son proyectos distintos y usan alias opuestos. Ambos controles de campo desde
fuentes pasaron 2/2 en CTest, ejecutando 42 en O0/O2
(`generic-owner-field-project-ctest.log`). El caso binario permanece fallando.

El control binario ahora incluye tanto un campo `PackOwner<1>` como un campo
`TypeOwner<int>`. Los dos fallan al seleccionar sus métodos en los ocho cruces
O0/O2: no es una limitación exclusiva de constantes. El consumidor sí contiene
las plantillas y registra la especialización del campo, pero la semilla tiene
`specializedSyntax` nulo porque la declaración concreta viene del binario. La
reconstrucción debe vincular la identidad completa del tipo físico, su plantilla
y sus argumentos; comparar solo el nombre terminal puede mezclar módulos.

La resolución de nombres confirma que los alias tienen prioridad sobre el
namespace léxico (`QualifiedNameLadder.cpp`, líneas 86-99). Por tanto no es
seguro elegir `Fixture.Value` solo por el namespace de origen: un alias en el
archivo podría denotar otro tipo. La identidad anidada observada no conserva
imports ni alias; el ensayo de nombre terminal único rechaza el caso válido con
homónimos y permanece incompleto.

El recorrido estático muestra que `processNestedTypeSyntax` visita primero el
argumento nominal y registra su identidad resuelta, mientras
`reserveTypeDemandWave` reserva las claves antes de reescribir las sintaxis.
`typeSpecializationIdentity` vuelve a escritura y origen cuando el genérico
anidado todavía no tiene identidad resuelta: la clave exterior puede perder
una identidad nominal disponible. Esta es una hipótesis basada en código, no
una corrección ni una prueba de colisión. Hay que contrastar alias opuestos en
archivos del mismo namespace y evaluar compatibilidad de metadata y caché antes
de cambiar las claves.

El contraste dirigido `A:/nsc-fixes-evidencia/nested-alias-identity-probe.cmd`
ejecutó `--dump-project-generics` con código 0. Dos archivos del mismo
namespace declaran `Owner<Nest<Value>>`, uno con alias `Value = First.Value` y
otro con `Value = Second.Value`. El resultado registra dos especializaciones
distintas de `Nest`, con identidades completas de `First.Value` y `Second.Value`,
pero una sola especialización de `Owner` y un acierto de caché con la clave
`Nest<Value>@origin-module[10]=AliasProbe;namespace[7]=Fixture`. La salida íntegra
está en `nested-alias-identity-probe.log`. Esto demuestra la colisión de claves;
no se ha demostrado por esta prueba una consecuencia en ejecución.

La acción siguiente es diseñar la identidad estructural de la demanda anidada
antes de reservar la exterior, sin confundir la escritura del usuario con la
identidad nominal de sus argumentos. Deben evaluarse sus efectos en nombres
físicos, productores y consumidores, metadata, caché, argumentos constantes,
alias y referencias binarias. No se considera corregido el caso #70.

Se ensayó registrar la identidad canónica `generic:` al recolectar una demanda
de tipo cerrado, antes de reservar la demanda exterior. La recuperación de un
argumento anidado de una biblioteca consulta esa clave exacta en el planificador.
El build incremental terminó con código 0. El dump de los dos alias ahora muestra
dos claves de `Owner`, una para `First.Value` y otra para `Second.Value`.

La nueva regresión `generic_owner_nested_alias_binary.py` comprueba las claves y
compila y ejecuta consumidores O0/O2 de productores estático y DLL en O0. El
conjunto dirigido `generic-owner-method-directed-ctest.cmd` pasó 6/6; la matriz
original `generic-owner-method-regression.cmd`, las restricciones dependientes,
la reconstrucción binaria y los controles léxicos también terminaron con código
0. Los comandos y logs están en `A:/nsc-fixes-evidencia`. La evidencia usa el
árbol con cambios locales, no una revisión limpia.

El cambio sigue provisional y sin commit. Hay que revisar argumentos genéricos
abiertos y modificadores anidados, el orden de demandas, la propagación anterior
para llamadas sin dueño identificable y el tratamiento de bibliotecas con claves
antiguas. En particular, el fallback de metadata antigua solo admite coincidencia
nominal no ambigua; la prueba de bibliotecas recién producidas no demuestra que
todas las bibliotecas antiguas se puedan consumir. No se ha cerrado el caso #70.

Un nuevo control de `TypeOwner<Nest<Value>[]>` encontró el diagnóstico
`generic method declaration is missing from its closed owner` en los ocho
cruces estáticos/DLL y O0/O2. El productor terminó correctamente. El dump
`nested-array-identity-probe.log` muestra que el argumento incluye `[]` después
de la clave del tipo `Nest`, pero la clave del planificador no incluye ese
modificador. El ensayo separa los sufijos de arreglo dinámico antes de buscar
la especialización exacta y los restituye en la sintaxis del argumento.

Tras el build incremental, `generic-owner-nested-array-ctest.cmd` pasó 1/1 con
los ocho cruces, y `generic-owner-method-directed-ctest.cmd` pasó 6/6. También
pasó la prueba dirigida de restricciones dependientes. Un caso de
`Outer<Inner<T>>` dentro del método de `Wrapper<T>` pasó en O0/O2 para dos
instancias distintas (`generic-owner-method-inference-ctest.log`). Estos
resultados no prueban todos los tipos abiertos, otras clases de modificadores,
el consumo de metadata antigua ni la propagación sin dueño identificado.

Al ampliar el mismo control con `TypeOwner<Value[]>` y `TypeOwner<int[]>`, el
productor terminó con código 0 y el consumidor falló al reconstruir el dueño de
`int[]`; el primer diagnóstico quedó en `generic-owner-nested-array-ctest.log`.
El dump `nested-array-identity-probe.log` identifica los tres formatos:
`generic:...[]`, `type:...[]` y `int[]`. El ensayo ahora separa los sufijos de
arreglo dinámico antes de clasificar el argumento y los agrega de nuevo a la
sintaxis generada, conservando la identidad completa cuando existe.

Con ese ajuste pasaron el build incremental, la prueba binaria de arreglos
en los ocho cruces O0/O2, los seis controles `owner-method-` y los controles
dirigidos `nsc.generics.binary-`. El log de la prueba binaria se sobrescribió
con el resultado posterior; el diagnóstico anterior consta en este registro.
Los arreglos de otros rangos, punteros y metadata antigua siguen sin demostrar.

Para `items[0].At<20,42>()` con `items` declarado `Own<1>[]` y un `Own<2>`
sin usar, el compilador rechazó en O0/O2 el índice del pack antes de sustituir
el parámetro del dueño. La inferencia de expresiones no reconoce indexadores y
la selección de llamadas caía en la ruta sin dueño cerrado. El ensayo obtiene el
tipo de la colección cuando está identificado como arreglo dinámico y copia el
tipo de su elemento únicamente para seleccionar el dueño de esta llamada.

Tras compilar incrementalmente, el caso válido ejecutó 42; el índice inválido
se rechazó con `outside a pack of length 2`, y dos arreglos de dueños distintos
ejecutaron 42, todo en O0/O2. Al añadir el negativo, el primer runner falló
porque su tabla esperaba un diagnóstico de receptor; se corrigió la tabla para
exigir el diagnóstico de índice efectivamente contratado. La ejecución final
de `generic-owner-method-inference-ctest.cmd` terminó con código 0 y el
conjunto `generic-owner-method-directed-ctest.cmd` pasó 6/6. Esto no acredita
indexadores de usuario, arreglos de otros rangos ni todas las rutas sin dueño.

El campo `value` de `Wrapper<const int N>` declarado como `Own<N>` reprodujo en
O0/O2 el mismo rechazo prematuro al llamar `Wrapper<1>.value.At<20,42>()`.
La traza temporal identificó la plantilla del contenedor, pero no un contexto
en el índice de campos ordinarios. `discoverTemplateDependencies` deja de
recorrer los hijos al encontrar una plantilla y conserva imports y alias en el
registro de esa plantilla. La traza temporal fue retirada.

El ensayo resuelve la plantilla del contenedor en el sitio de uso, normaliza
sus argumentos y los sustituye en la declaración del campo; emplea los imports
y alias del contexto donde se declaró. El build incremental y la reproducción
mínima terminaron con código 0. La prueba dirigida ejecutó 42 para un dueño
válido y otro no usado, distinguió dos dueños con constantes distintas y rechazó
el índice inválido, tanto en O0 como O2. La selección dirigida relacionada pasó
6/6. Los contenedores con packs, campos heredados y el consumo binario de esta
forma concreta permanecen sin verificar; el caso #70 sigue abierto y sin commit.

La prueba binaria de `GenericWrapper<1>.value.At<20,42>()` confirmó el rechazo
prematuro en los ocho cruces estáticos/DLL O0/O2, con productor exitoso. Una
traza temporal mostró que el contexto y la sustitución ya eran correctos:
el campo resultante era `PackOwner<const(builtin:int=1)>`. La especialización
recuperada del planificador era una semilla importada sin `specializedSyntax`.
La ruta ahora busca su declaración física por módulo, namespace, dueño y nombre
especializado, y usa esa declaración como dueño cerrado; se retiró la traza.

Después del build incremental, la prueba binaria ejecutó 42 en los ocho cruces
con un segundo dueño sin llamar y rechazó en los mismos cruces el índice
inválido de `GenericWrapper<2>`. La selección dirigida pasó 6/6. Esto no
verifica contenedores con packs, campos heredados ni metadata antigua; el caso
sigue sin commit y no se considera cerrado.

`Wrapper<1, int, long>` con pack y campo `Own<N>` reprodujo el rechazo
prematuro en O0/O2 porque el ensayo anterior excluía contenedores con pack.
La resolución usa ahora las sustituciones fijas y del pack durante la copia
del tipo del campo, y restaura después el estado previo de packs. En fuentes
pasaron ese caso con un dueño inválido no llamado y otro donde el campo se
selecciona mediante `packtype<Ts, 0>`.

La variante binaria mostró un fallo anterior en el productor: la verificación
de metadata indicaba `generic type 'packtype<...>'` como dependencia faltante.
`collectTypeDependency` buscaba una plantilla nominal para la construcción que
`clonePackType` maneja como intrínseca. La búsqueda ahora reconoce esa forma y
sigue inspeccionando sus argumentos. Después del build incremental, el productor
estático y DLL y los consumidores O0/O2 de ambas formas pasaron; la selección
dirigida terminó 6/6. El log previo fue sobrescrito por el resultado posterior;
el diagnóstico original queda registrado aquí. No se modificó interoperabilidad
C/C++, y el caso general sigue abierto y sin commit.

`Derived : Base` con un campo heredado `Base.value : Own<1>` reprodujo el
rechazo prematuro del índice del pack en O0/O2. La búsqueda del receptor solo
examinaba campos declarados por `Derived`. El ensayo conserva el contexto
léxico de las clases y recorre las bases nominales no genéricas por identidad,
desde la clase derivada hacia la base, deteniéndose ante un ciclo o una base
no identificada de forma única. La búsqueda del campo usa el contexto de la
clase que lo declara.

Tras el build incremental pasaron los casos de una base, dos niveles con alias
distintos entre declaración y consumidor, y el diagnóstico esperado para un
índice inválido en O0/O2. La variante importada pasó en los ocho cruces de
biblioteca estática/DLL y optimizaciones; el conjunto dirigido pasó 6/6.
Las bases genéricas y la metadata antigua no están cubiertas por estas pruebas.

Las variantes `Derived : Base<1>` y `Derived<M> : Base<M>` con campo
`Base<N>.value : Own<N>` también reproducían el rechazo prematuro en O0/O2. El
recorrido anterior se detenía en una base genérica. El ensayo copia cada base
con las sustituciones del derivado, resuelve su plantilla en el ámbito léxico
del tipo que la declara y transporta sus argumentos hasta el campo.

Tras el build incremental pasaron ambas variantes, dos dueños con constantes
distintas y el rechazo del índice inválido en O0/O2. La variante importada
`GenericDerived<N> : GenericBase<N>` pasó en los ocho cruces estático/DLL y
O0/O2. El conjunto dirigido `generic-owner-method-directed-ctest.cmd` pasó
6/6. La prueba utiliza bibliotecas recién producidas; la metadata antigua y
otras formas de receptor siguen sin verificar. El caso permanece sin commit.

Un receptor `items[0].value.At<20,42>()`, con `items` de tipo
`Wrapper<1>[]` y `value` de tipo `Own<N>`, se rechazó en O0/O2 por evaluar
`packat(Ns, N)` antes de sustituir `N`. La prueba conserva un `Wrapper<2>`
sin llamar para comprobar que solo se demanda el dueño correcto. El análisis
de campo consultaba el tipo de `items[0]` sin usar el tratamiento existente
para un elemento de arreglo dinámico. El ajuste provisional aplica esa misma
extracción al contenedor del campo y conserva el ámbito y la sustitución ya
usados para la declaración del campo.

El build incremental terminó con código 0. La reproducción pasó en O0/O2;
también pasaron los nuevos controles de dos dueños distintos y de índice
inválido. La selección `nsc.generics.owner-method-` pasó 6/6, con controles
de fuentes y binarios preexistentes. Comandos y resultados en
`A:/nsc-fixes-evidencia/generic-owner-array-field-{current,build,after,controls}.log`
y los `.cmd` correspondientes. Los controles binarios de la selección no
prueban específicamente esta forma de receptor de arreglo. El ajuste depende
del ensayo conjunto sin commit; no demuestra indexadores de usuario, arreglos
de otros rangos ni cierra el caso general.

La prueba `generic_owner_method_binary.py` incorpora ahora el receptor de
arreglo con el productor en biblioteca: valida una lectura y una operación
con `GenericWrapper<0>` y `GenericWrapper<1>` distintos; también exige el
diagnóstico del índice inválido para `GenericWrapper<2>`. Pasaron los ocho
cruces de biblioteca estática o DLL, productor O0/O2 y consumidor O0/O2.
Comando y salida: `A:/nsc-fixes-evidencia/`
`generic-owner-array-field-binary-current.cmd` y `.log`. El productor se
reconstruyó para cada configuración sin caché; esto no acredita metadata
antigua ni indexadores definidos por el usuario. El trabajo del caso #70
permanece provisional y sin commit.

Con el compilador aislado SHA-256
`40566c10872f48dd5c12909c68983b1f972493d3639ec65997e3c190502d41bd`, se
repitió la selección dirigida `generic-owner-current-directed.cmd`: pasaron
6/6 pruebas de inferencia, productor, referencias de proyecto y consumidores
binarios (`generic-owner-current-directed.log`). El ejemplo original intacto
`cases/70-method` pasó `--check-project-generics`; también compiló sin caché y
devolvió 6 en O0 y O2, el resultado esperado de `CellCount` para 2 por 3.
Los comandos y salidas están en `generic-owner-original-current.log` y los
archivos `generic-owner-original-O*-build.log` y `*-run.log`. El `Main` original
no llama a `Multiply`; las pruebas dirigidas adicionales sí ejercitan métodos
genéricos y consumo binario. El resultado usa el árbol con cambios sin registrar
y no acredita todas las formas de receptor. El conjunto permanece sin commit.
