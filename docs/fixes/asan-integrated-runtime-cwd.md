# Directorio de ejecución de la prueba dirigida del runtime ASan

`tests/integration/address_sanitizer_integrated_runtime.py` ejecutaba el
constructor de pruebas, Clang y los ejecutables generados desde directorios
temporales. Los lineamientos vigentes exigen iniciar los procesos desde el
repositorio de trabajo. Se ha sustituido el directorio inicial de esos hijos
por la raíz del repositorio; los argumentos de rutas y directorios temporales,
los casos y las aserciones permanecen intactos. Las DLL continúan junto a los
ejecutables generados. No se han modificado el runtime ni su adaptador.
Ambos fixtures incluyen ahora el directorio de ejecución de cada hijo en el
registro para que la procedencia pueda comprobarse junto al comando y salida.

La prueba se ejecutó después con el ejecutable construido desde esta copia y sus
dependencias autorizadas; los resultados están en la tabla siguiente.
La comprobación sintáctica dirigida `python -B -m ast
tests\integration\address_sanitizer_integrated_runtime.py`, iniciada desde la
raíz con `A:/nsc-asan-runtime-evidencia/check-syntax.cmd` y temporales en A,
terminó con código 0. No ejecuta el fixture.

En `address_sanitizer_heap.py`, las llamadas habituales también se inician
desde el repositorio. El control de shadowing conserva el ejecutable falso en
el directorio actual del compilador y la misma aserción; para esa subprueba,
`NSC_TEST_ASAN_SHADOW_CWD` permite seleccionar el directorio autorizado. Si ya
existe allí `llvm-objcopy.exe`, la prueba falla sin sobrescribirlo; el archivo
creado se retira en `finally`. Si no se configura, conserva su directorio
temporal previo para ese caso. La prueba se ejecutó cuando el directorio ya no
estaba compartido con un build activo. La
excepción comunicada para esta prueba señala exclusivamente el directorio de
build aislado `A:/nsc-asan-sdk-evidencia/build`; no se extiende a otros cwd.
La comprobación `python -B -m ast tests\integration\address_sanitizer_heap.py`
terminó con código 0 desde la raíz y con temporales en A mediante el mismo
script de comprobación sintáctica. Tampoco ejecuta la prueba funcional.

El argumento `example` permite ahora pasar el directorio original del caso 97;
los dos archivos `.ll` auxiliares se resuelven por separado desde el repositorio.
El fixture copia el ejemplo y proyecto originales a un temporal y no los modifica.
Antes de la ejecución funcional, `certutil -hashfile ... SHA256`
registró `2b20554fda04f5d2890ceade5f760353df58eb54d8c14837a59666c6387895d2`
para `Program.ns` original y
`f714bad022daf07265ed7716ed4062d7d6dec6aa7b33e8c1ff72382346969583`
para su proyecto. El duplicado versionado `Program.ns` tiene hash
`2158174334ab33d9ae5807016708c9405ed55b41f4f413867b7d933a8d01ef67`:
`fc /b` detecta diferencias de finales CRLF/LF y `fc` textual no encuentra
diferencias. Los hashes del programa y proyecto originales se comprobaron de
nuevo después de la prueba y coinciden con los registrados antes. Ninguno se
modificó ni corrigió.

## Resultados dirigidos

El 12 de noviembre se ejecutaron desde `A:/nsc-limitaciones-asan` las cuatro
comprobaciones siguientes, con temporales propios en `A:/nsc-asan-runtime-evidencia`,
`NSC_LINKER=nslink.exe`, Clang externo `C:/clang-llvm/bin/clang.exe` y
`ASAN_OPTIONS=detect_leaks=0`. Cada script `.cmd` conserva las variables y el
comando exactos; cada log conserva comandos, directorios, salidas y diagnósticos.
No se ejecutó ninguna batería completa.

| Comando | Resultado | Evidencia y alcance |
|---|---|---|
| `A:/nsc-asan-runtime-evidencia/run-integrated.cmd` | 0 | `integrated.log`: fixture sano devuelve 42 en O0/O2; el inválido produce `heap-buffer-overflow` en O0/O2. La falla de reserva y realloc conserva el bloque y devuelve 42. |
| `A:/nsc-asan-runtime-evidencia/run-heap.cmd` | 0 | `heap-original.log`: el programa original produce `heap-buffer-overflow` en O0/O2; el control válido devuelve 42 con y sin ASan; el uso tras liberación se detecta en O0/O2. Los dos runtimes incompatibles se rechazan y el control de shadowing pasa. |
| `A:/nsc-asan-runtime-evidencia/run-incompatible.cmd` | 0 | `incompatible.log`: ocho casos de heap, punteros, tamaños y flags no admitidos terminaron con salida 87 y el diagnóstico esperado. |
| `A:/nsc-asan-runtime-evidencia/run-throwing-array.cmd` | 0 | `throwing-array.log`: los cuatro programas para arreglos de estructuras y clases, en O0/O2, compilaron y ASan detectó el acceso posterior a la liberación después de un destructor que lanza. |

Para shadowing se comprobó que `A:/nsc-asan-sdk-evidencia/build/llvm-objcopy.exe`
no existía antes y que no quedó después. Su creación exclusiva y eliminación
fueron parte del fixture; no se ejecutó el test mientras Ninja estaba activo.
La excepción para cwd aplica a esa prueba concreta y fue coordinada con SDK.

El compilador usado, construido en la copia de esta rama, tiene SHA-256
`49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`;
el runtime independiente usado por las pruebas de driver tiene
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.
Ambos coinciden con los hashes registrados por SDK antes y después. El ejecutable
de pruebas integrado tiene hash posterior
`36ca1ef5a67e67dffc2a951bedf1c53e62dcec4fa9cdb8f71a5a547840b2e547`;
la DLL ASan externa tiene hash posterior
`e078bb324a2d08023161fd5eab75b8adf3e6bd9e3086e0096a03b2d59d721c22`.
El objeto adaptador producido por la prueba de argumentos incompatibles tiene
`132ce63bc42c8b7a344be9b3b29feecb4d23090c0a5e8d0fa36816a193531cf5`.

Estas pruebas no demuestran ausencia general de fugas, todas las combinaciones
de parámetros ni instrumentación de bibliotecas binarias o del runtime. El
fixture de fallo de asignación utiliza IR de apoyo; los otros accesos reportados
son de los programas concretos indicados en sus logs. Clang informó el aviso de
reemplazo del triple al compilar IR, sin impedir el resultado observado.
Para la prueba de destructor se ajustó únicamente el cwd de sus hijos en
`asan_throwing_array_release.py` y se añadió al log; no se modificó su fuente
generada, aserciones ni código de punteros. La observación del acceso a memoria
liberada comprueba ese recorrido concreto, no ausencia general de fugas.
