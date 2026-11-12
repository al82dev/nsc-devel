# Procedencia de herramientas y alcance de la evidencia

Durante la integración se constató que algunos agentes ejecutaron copias
experimentales de `nsc.exe` mediante rutas propias en A. Los lineamientos
exigen herramientas por nombre mediante el PATH vigente y la excepción
expresamente autorizada para el compilador es
`A:/nsc-fixes-evidencia/build/bin/nsc.exe`. Por ello, las ejecuciones con las
copias de los agentes se conservan como evidencia técnica de investigación,
pero no se consideran validación conforme. Esto incluye los compiladores
híbridos y congelados identificados en los informes de agentes de genéricos,
capturas y excepciones. Los hashes y resultados anteriores siguen reflejando
lo que se observó; no deben confundirse con una validación aceptada.

Las comprobaciones denominadas «verificación integrada» en los informes de
esta tanda se repitieron con el compilador autorizado de A, el runtime aislado
de A y temporales en A. Esas pruebas utilizaron `nslink.exe` por nombre: el
PATH vigente lo resolvió a `C:/native-sharp/bin/nslink.exe`, como herramienta
externa de solo lectura. No acredita que NSLink proceda de la copia del
compilador. Ninguna de esas comprobaciones certifica una revisión limpia cuando
el propio informe indica que había cambios locales.

Una prueba exploratoria adicional invocó una copia de NSLink por ruta en A;
no se considera validación conforme. El agente informó el incidente y dejó de
usar esa copia. Las pruebas posteriores deben usar herramientas por nombre,
salvo las excepciones concretas ya autorizadas por el usuario. Los resultados
históricos de otras tandas conservan su procedencia y necesitan una revisión
individual antes de presentarse como evidencia conforme.

Revisión posterior: algunos ejecutores iniciaron procesos hijos con el
directorio de trabajo en temporales de A, aunque el comando exterior partiera
del repositorio. Esas ejecuciones se conservan como observaciones no conformes
con la regla de iniciar cada comando desde la copia autorizada. También se
detectaron pruebas CTest que invocaron CMake o Python por rutas absolutas de
configuración, sin autorización específica, y una consulta `where` de la
ubicación de `nslink`. No se cuentan como validación conforme. Los controles
de alias, limpieza débil y construcción fallida identificados en sus informes
se repitieron tras corregir sus ejecutores; el detalle está en
`A:/nsc-fixes-evidencia/repo-cwd-corrections-20261112.json` y sus logs.
Las demás pruebas deben revisarse individualmente: un comando exterior iniciado
desde el repositorio no demuestra por sí solo el directorio de sus hijos.
El agente que preparó las pruebas de limpieza confirmó además que sus
comandos exteriores originales se iniciaron desde `A:/`, no desde su copia;
esas mediciones tampoco se consideran conformes. Sus repeticiones posteriores
desde el repositorio principal se identifican por separado.

Los registros nuevos de visibilidad y de las dos pruebas ASan de limpieza se
ajustaron para invocar `python` por nombre y establecer el directorio de
trabajo del repositorio. Las repeticiones documentadas se ejecutaron
directamente; la nueva configuración CTest aún no se ha ejecutado.
