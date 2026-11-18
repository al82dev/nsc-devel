# SIMD: verificación dirigida tras la integración

Revisión `9925123a0`, compilador aislado
`A:/nsc-fixes-evidencia/build/bin/nsc.exe` y Core O0 aislado. No se modificó el
compilador ni se ejecutaron pruebas de interoperabilidad C/C++.

La incidencia histórica de `89-mask-laptop-02-verification.md` describía un
fallo de layout al poner SIMD o máscaras dentro de tuplas. Al repetir su
reproducción con el compilador actual, los controles desde fuente y los cruces
binarios para SIMD directo, tupla SIMD y tupla anidada de máscara compilaron y
ejecutaron 42 en O0 y O2. Esa reproducción devuelve una constante y por sí sola
no prueba que los valores se conserven. Salida:
`A:/nsc-fixes-evidencia/simd-metadata-tuple-integrado-20261112.log`.

Para comprobar los valores se añadió `tests/integration/simd_tuple_binary_roundtrip.py`.
Un productor recibe una tupla SIMD, lee las posiciones primera y última, y
recibe una tupla anidada de máscara con posiciones verdaderas y falsas. Los
controles desde fuente y las cuatro combinaciones de productor y consumidor
O0/O2 ejecutaron 42. Los comandos, proyectos y salidas están en
`A:/nsc-fixes-evidencia/simd-valores-binarios-mixtos-20261112/`; el resumen
está en el archivo del mismo nombre con extensión `.log`. El script exige una
carpeta nueva de evidencia en A y falla si algún resultado difiere.

`simd_aggregate_iterator_layout.py` pasó 30 combinaciones dirigidas: tres
anchuras, cinco formas de agregado o protocolo y O0/O2. Revisa valores de
lanes, tuplas y máscaras, así como iteradores y futuro; su salida está en
`A:/nsc-fixes-evidencia/simd-agregados-integrado-20261112.log`. La matriz de
máscaras pasó 86 formas válidas, 10 inválidas, índices, conversiones inválidas y
los dos cruces binarios, según `simd-matrix-integrado-20261112.log`.

El resultado cierra solamente las reproducciones y combinaciones anteriores:
no acredita todas las anchuras, firmas, optimizaciones ni las rutas de alias o
concurrencia. El estado previo reflejaba una revisión anterior; aquí no se
atribuye la corrección a un commit concreto. Se usaron exclusivamente pruebas
dirigidas, sin batería completa.
