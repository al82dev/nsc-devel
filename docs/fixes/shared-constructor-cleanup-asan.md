# Construcción fallida de controles compartidos con ASan

La prueba `shared_implicit_constructor_cleanup.py` comprueba fallos de inicialización antes y después de construir un campo propietario, y un constructor exitoso. La variante opcional `nsc.sdk.shared-implicit-constructor-cleanup-asan` usa el mismo fixture para controles local y atómico, cargas clase y struct y optimizaciones O0/O2. Compila doce fuentes mínimas del SDK en un proyecto temporal, sin instrumentar Core binario.

La variante recibe `NSC_TEST_ASAN_CLANG` y `NSC_TEST_ASAN_RUNTIME`; si falta alguno devuelve 77 para una omisión explícita. Fuerza `detect_leaks=0`, por lo que no demuestra ausencia de fugas ni liberación física de todos los bloques. Los ocho escenarios devuelven 42 y no producen diagnóstico ASan; esto no acredita otros constructores ni todas las familias públicas.

El 11 de noviembre, con el compilador aislado SHA-256 `15b3e955f7eab1b471bdd2ecdfc82a35b3b1bd65239ffbd5ddd4d91f2cc36c26`, pasaron ocho combinaciones ASan y ocho normales (aproximadamente 6,69 y 2,03 segundos). Los registros separados son `A:/nsc-agent3-evidencia/hybrid/shared-constructor-permanent-1.log` y `shared-constructor-permanent-0.log`. El sondeo sin dependencias devolvió 77 sin invocar el compilador (`shared-constructor-skip.log`). Se ejecutó el script dirigido directamente, no CTest; el compilador y las fuentes incluían otros cambios locales.

Corrección: esas invocaciones se iniciaron desde `A:/` y sus procesos hijos
desde un temporal, por lo que son evidencia no conforme. Con el ejecutor
corregido se repitieron desde el repositorio principal ocho controles ASan y
ocho normales, todos con salida 42. Los comandos y salidas están en
`A:/nsc-fixes-evidencia/repo-cwd-corrections-20261112.json` y logs
`repo-cwd-correction-constructor-{0,1}-20261112.log`. Se mantienen los límites
de Core binario y `detect_leaks=0`.
