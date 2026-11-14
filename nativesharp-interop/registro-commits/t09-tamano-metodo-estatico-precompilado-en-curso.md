# T09-003: método estático con tamaño oculto en plantilla precompilada

Base: `770541a89`. Este conjunto implementa y prueba el caso descrito; el ítem general T09-003 permanece abierto.

La captura y reconstrucción de métodos estáticos se está probando para una plantilla NativeSharp publicada como biblioteca. Inicialmente la prueba original falló en O0 cuando el consumidor también declara el encabezado C++: `duplicate C++ static method signature or symbol`. Evidencia: `A:/temp/nsc-t09-object-size-pre-c7z7d0pj/O0-con-encabezado-build.log`.

Para aislar la otra ruta se copió el fixture a `A:/temp/t09-static-method-noheader-isolated` y en esa copia se seleccionó únicamente `sin-encabezado`; la prueba del repositorio permanece intacta. Pasaron cuatro consumidores precompilados estáticos y cuatro de DLL, O0 a O3, y los controles directos correspondientes. Evidencia: `A:/temp/nsc-t09-object-size-pre-2vlpout3`, `A:/temp/nsc-t09-object-size-pre-xcxn_twd` y `A:/temp/nsc-t09-size-static-method-7q__mzf2`. La línea del fixture que dice ocho consumidores no se actualizó en la copia; para cada biblioteca se ejecutaron cuatro, según sus logs.

La validación de miembros rechaza símbolos duplicados globalmente. Antes de admitir dos observaciones es necesario demostrar identidad de declaración y de propietario, compatibilidad de firma y procedencia de la biblioteca: el mismo símbolo o USR por sí solo no distingue dos DLL diferentes. La selección y el lowering de ambos propietarios y la declaración LLVM compartida también deben comprobarse. No se considera resuelto el consumidor con encabezado ni se certifican otros casos de métodos C++.

## Avance: conciliación con encabezado

La compilación incremental de `nsc` pasó. La prueba dirigida ampliada llama al método tanto desde el consumidor como desde la plantilla precompilada: pasaron O0–O3, con y sin encabezado, para biblioteca estática y DLL. Evidencias: `A:/temp/nsc-t09-object-size-pre-9tw6g_p3`, `A:/temp/nsc-t09-object-size-pre-gkogeinz` y `A:/temp/nsc-t09-size-static-method-q_r4xv1a`.

Un control aislado construyó un `.lib` distinto que conserva el mismo encabezado y símbolo; NSC lo rechazó con `shared C++ static method library differs`. Evidencia: `A:/temp/t09-static-distinct-lib-a89099dc2a40460b8174366e7ae00401/resultado.log`. Falta incorporar este control negativo al fixture permanente, probar diferencias de DLL y firma y ejecutar las regresiones pertinentes antes del commit. Se mantiene abierto T09-003.

La prueba permanente ahora ejecuta ambas llamadas y construye una biblioteca distinta para comprobar el rechazo. La variante genérica pasó completa con ese control (`A:/temp/nsc-t09-size-static-method-068jmguh`), y la no genérica también pasó (`A:/temp/nsc-t09-size-static-method-i6my6g0y`), cada una con consumidores estáticos y DLL en O0–O3. Siguen pendientes los controles de diferencias de DLL y firma y las regresiones antes del commit.

## Resultado y límites del conjunto

La prueba permanente `validar-metodo-estatico.py --precompilado` pasó para O0–O3, biblioteca estática y DLL, con y sin encabezado, incluidas llamadas directas y mediante la plantilla. También rechaza en O0 un archivo `.lib` distinto con el mismo símbolo, una firma con `noexcept` distinta y una DLL modificada. Evidencia completa: `A:/temp/nsc-t09-size-static-method-301lpvhn`. La variante no genérica pasó por separado en `A:/temp/nsc-t09-size-static-method-i6my6g0y`.

La prueba de identidad `nsc_cpp_c_identity_tests` pasó e incluye restauración del catálogo con método y enumerador simultáneos, rechazo de identidad vacía, duplicados y truncamiento. Las cinco pruebas CTest seleccionadas de observación de records y planes ABI pasaron; dieciséis consumidores de la prueba de funciones libres pasaron con biblioteca estática y DLL, O0–O3, con y sin encabezado (`A:/temp/nsc-t09-object-size-pre-pypc0zhe`, `A:/temp/nsc-t09-object-size-pre-118pc_ff`).

El productor registra los métodos estáticos referidos por plantillas publicados para records C++ completos de valor trivial. Al conciliar con una declaración viva se exigen USR de propietario y método, tipo canónico, firma proyectada y huellas SHA-256 del archivo enlazado y, si corresponde, de la DLL. Bibliotecas equivalentes con bytes distintos se rechazan conservadoramente. La reconciliación entre varios productores precompilados sin observación viva, métodos de instancia, clases no triviales y otras firmas no queda acreditada. Los controles no certifican interoperabilidad C++ general ni cierran T09-003.

Tras endurecer la comprobación para observaciones del mismo proyecto, se repitieron la compilación incremental, la prueba de identidad y la prueba dirigida completa; todas pasaron. La evidencia final de casos positivos y de los tres rechazos es `A:/temp/nsc-t09-size-static-method-ryad5nfq`.
