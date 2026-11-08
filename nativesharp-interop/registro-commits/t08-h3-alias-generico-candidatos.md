# Tanda 8: conservar candidatos de alias de una plantilla

La fase de nombres omite los cuerpos de plantillas genéricas sin instanciar.
Por eso no existe todavía un binding que confirme el typedef observado en la
reproducción `fc0fc47da`. La captura conserva sus observaciones C como
candidatas, con su módulo y referencia, para que se resuelvan más adelante. Si
ya existe un binding, utiliza únicamente el alias que este haya seleccionado.

La compilación incremental y `nsc_cpp_c_identity_tests` terminaron con código
0, incluida la reproducción con una referencia C real en el proyecto y el
formato preparado 46. El cambio se verificó antes de un ajuste posterior de
indentación y del texto descriptivo de la aserción.

La emisión normal del formato, el consumidor completo y las colisiones con
parámetros, tipos y ámbitos NativeSharp siguen pendientes de validación. Que
un candidato se conserve no demuestra que sea el alias seleccionado ni que la
plantilla completa sea interoperable.
