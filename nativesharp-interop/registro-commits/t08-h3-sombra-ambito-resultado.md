# T08 H3: resultado inicial del control de ámbito

Con el compilador de desarrollo disponible en `A:/nsc-interop-build/tanda1-ON`,
`validar-ambito.cmd` terminó con código 0. El productor, la verificación de su
metadata y el consumidor precompilado funcionaron en O0, O1, O2 y O3; el
consumidor comprobó el resultado 179 en cada nivel.

Esto acredita que el caso de uso legítimo existe antes de la corrección y sirve
como regresión para esta forma de bloque. No corrige la captura indebida del
fixture `Local`, ni acredita otros ámbitos o la validación completa de la tanda.
