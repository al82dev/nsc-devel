# Experimento de localización de TLS exportado en PE

## Evidencia

`pruebas-tanda-08-tls-c/directo/validar.cmd` falla en el enlace del
consumidor C: Clang emite `IMAGE_REL_AMD64_SECREL tls_exported` y la biblioteca
de importación no satisface ese símbolo. El fixture original permanece.

El experimento separado `validar-pe-tls.cmd` construye la misma DLL con Clang
y LLD y ejecuta un consumidor C de Windows x64. Localiza el símbolo exportado
en la imagen, obtiene el directorio TLS de PE y su índice, y consulta los
slots TLS mediante `GS:0x58`. Calcula el desplazamiento respecto al inicio
de los datos TLS de la imagen. El programa leyó la inicialización, escribió
en el hilo principal y en otro hilo, comprobó ambos valores mediante la función
exportada por la DLL y verificó el aislamiento. Salida: `Experimento PE TLS
directo: lectura y escritura en dos hilos correctas.`

## Límites y siguiente paso

Esto es evidencia experimental, no una solución de producto: depende de
detalles internos de Windows x64 y de una DLL que exporta el dato como símbolo
PE. Falta comprobar compatibilidad, vida de la DLL, protección ante direcciones
inválidas y cómo incorporarlo a la representación de globales y al descenso de
NSC. No se ha probado con otros módulos, TLS dinámico ni otras arquitecturas.
La prueba original sigue fallando y la tanda 8 permanece abierta; no se cambió
NSLink ni el compilador. Las salidas se guardaron en
`A:/nsc-interop-build/tanda08-tls-directo`.
