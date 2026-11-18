# Construcción predeterminada de struct

LKF-343: un struct con constructores explícitos conserva su construcción predeterminada.
La comprobación de accesibilidad rechazaba el sitio antes de evaluar ese caso cuando todos
los constructores explícitos eran privados. Ahora se reconoce la construcción predeterminada
sin argumentos primero; el acceso privado sigue rechazado cuando se pasan argumentos.

La prueba dirigida reproduce el rechazo anterior y comprueba el binding predeterminado,
un constructor parametrizado público y el rechazo del privado con argumentos. El blanco
`nsc_project_construction_model_tests` se compiló incrementalmente y pasó 78 comprobaciones.

Un proyecto separado construyó el struct desde otro tipo y comprobó su campo cero. El
compilador de la copia emitió ejecutables O0 y O2; ambos terminaron con el código esperado 42.
Se utilizó el runtime generado en el build aislado y el enlazador externo autorizado. La primera
emisión falló porque el runtime predeterminado no existía; se repitió con el runtime de A.
