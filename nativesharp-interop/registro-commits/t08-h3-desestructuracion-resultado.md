# T08 H3: resultado del control de desestructuración

El control `ee469d31b` falló en O0–O3 antes de la corrección: se publicaba la
global C `shadow_value` a pesar de estar sombreada por una desestructuración.
Tras `ef1ab4cbe` seguía fallando; el patrón de declaración también se recogía
como candidato. `f69da2b97` corrige ese segundo caso.

Se construyeron incrementalmente `nsc` y `nsc_cpp_c_identity_tests`, con build
`359fb658942d547c`. `pruebas-tanda-08-sombras-plantilla-c/diagnosticar.cmd`
terminó con código 0: en O0–O3 generó y consumió la biblioteca, verificó el
resultado y la metadata y confirmó que no se emitieron las referencias C
sombreadas. No se ejecutó la batería completa de la tanda 8, que sigue abierta.
