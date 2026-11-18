# Alcance: campos y construcción genérica

La solicitud autoriza investigar, diseñar, implementar y verificar el caso 74 de
introspección estática y la construcción genérica de tipos internos mediante Core
binario, conforme a `TAREAS-CHAT-CAMPOS.md`. Se conservan los ejemplos originales.
Las decisiones de diseño pendientes se toman autónomamente y se justifican con
evidencia. La revisión inicial es `3f51a03aacd729a55321868ef47f9b7ae817f635`;
la base compartida es `d00b3a8698d7750c80887d029d64fda481d67211`, sin certificar.

Se permiten cambios causalmente necesarios en metaprogramación, parser,
genéricos, fases y pruebas de estos dos trabajos, coordinando archivos con los
otros frentes. `CMakeLists.txt`, `OopLlvmImplementation.h` y el estado general
de campaña son del integrador y no se modifican aquí. Se protegen los contratos
de visibilidad, el rechazo de constructores inaccesibles y los ejemplos previos.

Solo se permiten compilaciones incrementales y pruebas dirigidas; no se ejecutan
baterías completas. Los efectos de build, temporales y evidencia permanecen en
A:. No se modifican `A:\nsc-devel`, `C:\native-sharp`, builds de interop,
LLVM/Clang o herramientas externas. Se excluyen interoperabilidad C/C++, Daikon,
la fachada aplazada `NativeSharp.Text`, versiones ficticias, merge, rebase,
push y despliegue. Los defectos de linker o debugger se documentan en
`A:\gaps-limitaciones` para el integrador.

El cierre exige evidenciar cada reproducción con herramientas y SDK de esta
copia, implementar ambos contratos sin aproximaciones, pasar las pruebas
dirigidas positivas y negativas pertinentes, documentar decisiones y límites,
crear commits coherentes y comunicar al integrador dependencias y conflictos.
Ningún resultado histórico certifica la revisión actual.
