# Tanda 8: límite del consumidor C directo de TLS de DLL

El control C de `pruebas-tanda-08-tls-c/directo/validar.cmd` declara `extern __declspec(thread) int tls_exported` y Clang produce una referencia COFF `SECREL tls_exported`; la biblioteca de importación ofrece `__imp_tls_exported`, por lo que LLD informa el símbolo no definido. Esto consta en `A:/nsc-interop-build/tanda08-tls-directo/probe-link.log` después de ejecutar ese control.

No basta añadir `dllimport` a la declaración C. En la fuente de referencia de Clang 23.1.0, `A:/llvm-project-al82dev/clang/lib/Sema/SemaDecl.cpp`, la comprobación de `DLLAttr` y `getTLSKind()` emite `err_attribute_dll_thread_local` para esta variable; el texto está definido en `clang/include/clang/Basic/DiagnosticSemaKinds.td`. El experimento previo `tls-c-dll-directo.md` comprobó el rechazo en ejecución. La excepción del código de Clang para un estático local dentro de una función DLL no corresponde a este global.

Esta evidencia limita específicamente el control C compilado con el Clang oficial y el ABI del experimento; no demuestra que NativeSharp no pueda resolver el acceso mediante su propia integración PE. La prueba original sigue intacta y fallando. No se modificó Clang, NSLink ni se sustituyó el control por uno que oculte el fallo.
