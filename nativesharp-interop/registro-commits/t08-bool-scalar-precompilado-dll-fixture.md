# T08: global C bool precompilado a través de DLL

El control prepara una DLL NativeSharp que consume el productor estático y un
ejecutable que utiliza esa DLL. El header C permanece oculto durante ambas
compilaciones; el control de error lo restaura antes de salir. El programa
compara lectura, escritura y modificación compuesta con funciones C en O0–O3.

Este fixture está preparado sin ejecutar; no acredita la ruta DLL ni el acceso
directo a un global exportado por una DLL C. No modifica NSLink.
