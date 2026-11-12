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
