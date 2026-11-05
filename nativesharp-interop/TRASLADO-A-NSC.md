# Traslado del trabajo de interoperabilidad a NSC

El 5 de noviembre de 2026 se trasladaron a `A:/nsc-devel/nativesharp-interop`
los documentos, pruebas y evidencia que se habían registrado por error en el
fork de LLVM. Esta carpeta pertenece al repositorio de NativeSharp: el trabajo
consiste en adaptar NSC para consumir C y C++ con Clang oficial.

Se reprodujeron en `development-cpp-clang` los 108 commits desde la base LLVM
`ea7d852a70e8bdfaf601d6626a760f9771b2c4b4` hasta `729c08a1c1a2`, conservando
autor, fecha y mensaje. Los hashes nuevos difieren porque sus padres ahora son
commits de NSC. Antes de ajustar rutas, ambos árboles de la carpeta coincidían:
`ffb57507778d3bc7b009042f14360e9eaffe5280`.

Los scripts activos y la prueba de unidad apuntan a la ubicación nueva. Las
evidencias y descripciones históricas mantienen las rutas que tenían cuando se
capturaron; no se deben interpretar como rutas de trabajo vigentes. El código
fuente LLVM/Clang sigue utilizándose como referencia de solo lectura.

La rama anterior de LLVM se conserva como antecedente sin reescribir ni borrar
su remoto. Se selecciona una rama de fuentes limpia desde la base 23.1.0, que no
contiene esta carpeta. El traslado no cambia el estado de validación de ninguna
tanda ni demuestra que todas las pruebas se hayan ejecutado en la ruta nueva.
