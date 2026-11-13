# Plan Clang para llamada directa preserve_none

La sesión captura el plan ABI de una función C++ `preserve_none` que recibe y devuelve el mismo registro, incluso si es la única convención especial del encabezado. El plan se valida frente al símbolo y la identidad, tamaño y alineación del registro. El catálogo NSMeta vigente lo transporta en el campo existente y lo vuelve a validar al restaurarlo. El HIR permite la forma observada cuando el plan y el tipo coinciden, sin ampliar por mera coincidencia de tamaño.

El fixture `pruebas-tanda-09-preserve-none-record44-directo` pasó ocho comparaciones Clang 23.1.0/NativeSharp, directas y por puntero, en estática y DLL O0–O3 (`A:/temp/nsc-t09-preserve-cpp-record24-kwuwwstq`) y dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-preserve-cpp-record24-precompilado-uf06shrx`). El control C++ usa una rutina auxiliar de copia por enlazarse sin CRT.

La compilación incremental y `nsc.cpp-interop.regcall-record-plan` pasaron, incluidos el encabezado con solo `preserve_none`, la ida y vuelta del plan en el catálogo y el rechazo de una convención o identidad alteradas. La prueba negativa se conserva para una firma de registro con argumento adicional (`A:/temp/nsc-t09-preserve-none-record-negativo-_euua6aq`).

El resultado acredita esta muestra en Windows x64 y la familia de planes que supera las mismas comprobaciones, no todas las firmas o disposiciones de C++. Siguen pendientes otros argumentos, clases no triviales, variádicas y TLS aislado. T09-003 y la tanda 9 permanecen abiertos.
