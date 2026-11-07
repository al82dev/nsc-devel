# Record usado solo en un callback global C

El fixture usa `var factory = record_factory_global` y llama el callback sin
nombrar el alias ni el record C del resultado. Comprueba si la importación del
global selecciona la dependencia de tipo que aparece dentro de la firma y
permite leer su campo `value`.

Con NSC build `3f82235fd35e7b9a`, el proyecto llegó a IR con código 0; el IR
se guardó como `A:/nsc-interop-build/tanda08-callback/record-factory-inferred.ll`.
La ejecución O0–O3 se agregó al script agrupado; en aquel momento permanecía
pendiente. El caso no cubre otras firmas, metadata ni condiciones de colisión
de tipos.

## Validación posterior

El gate completo ajustado terminó con código 0 el 2026-11-07. Su registro
`A:/nsc-interop-build/tanda08-integral-ajustada-20261107.log` contiene los
marcadores `Record de callback global inferido O0`, O1, O2 y O3, cada uno
emitido después de que el ejecutable correspondiente devolvió cero. El script
es `pruebas-tanda-08-callback/validar.cmd`. La evidencia cubre este caso y esas
configuraciones; no cierra T08-017 ni la tanda.
