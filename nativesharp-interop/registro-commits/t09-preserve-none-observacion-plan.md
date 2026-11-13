# Observación del plan ABI preserve_none

La sesión C++ de NSC puede solicitar a Clang 23.1.0 el plan ABI de un alias de puntero `preserve_none`. Se añade un validador de la forma Win64 de un registro pasado y devuelto, que exige la convención 21 y reutiliza las comprobaciones físicas de la forma Win64 existente. Se mantiene separado el validador `regcall`, que sigue exigiendo la convención 92.

La prueba `nsc.cpp-interop.regcall-record-plan` observó en Clang un alias de registro compacto de ocho bytes y otro de 44 bytes por retorno y argumento indirectos. También comprobó rechazos al alterar convención, variadicidad, paso por valor y alineación. La compilación incremental y la prueba dirigida terminaron correctamente.

Esta pieza solo observa y valida el plan cuando se solicita; no habilita nuevas firmas al consumidor ni sustituye las comprobaciones de identidad y elegibilidad del registro. La captura y el transporte posteriores se documentan [aquí](t09-preserve-none-plan-nsmeta.md); la conciliación en el emisor permanece pendiente. T09-003 y la tanda 9 siguen abiertos. No se modificó Clang.
