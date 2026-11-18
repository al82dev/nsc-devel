# Alcance: igualdad decimal con escalas distintas

- Objetivo: corregir el resultado de `checked(0.10m + 0.20m) == 0.300m`
  reproducido por el coordinador con el compilador autorizado en O0 y O2.
  `A:/nsc-fixes-evidencia/prepared-source-controls-20261111/results.json`
  registra los comandos y salida 1. No modificar esa reproducción.
- Naturaleza: comparación numérica de igualdad decimal. Preservar operaciones
  aritméticas, conversiones, comparaciones de orden y demás tipos; no modificar
  el diseño de `decimal`, interoperabilidad C/C++, dependencias externas,
  la rama `development` ni `A:/nsc-devel` o `C:/native-sharp`.
- Causa estática: `applyComparisonOperation` compara como i128 empaquetado el
  coeficiente, escala y signo. Valores iguales con escala diferente tienen bits
  distintos. Confirmar con controles independientes de igualdad y suma.
- Archivos candidatos: `src/LLVM/Lowering/OopLlvmOperators.cpp`, declaración
  correspondiente en `src/LLVM/Internal/OopLlvmImplementation.h`, prueba
  dirigida y su documentación. Son fuentes del compilador NativeSharp, no de
  LLVM/Clang externos. El header puede coincidir con trabajo de interoperabilidad
  en otra rama y debe informarse antes de integrar.
- Algoritmo previsto: comparar coeficientes de 96 bits tras alinear escalas
  en 320 bits mediante shifts y sumas; hasta 63 diferencias de escala caben
  porque `96 + ceil(log2(10^63)) < 306`. Tratar ambos ceros como iguales y
  comparar signo cuando el coeficiente es distinto de cero. No se modifica el
  orden relacional ni se presume que el parser y el contrato semántico acepten
  el mismo rango de escala.
- Validación integrada: build incremental y pruebas O0/O2 con valores y escalas
  diferentes, `!=`, signos, ceros, diferencias de escala de hasta 28 y coeficientes
  amplios; el enlace pasó. La escala codificada máxima de 63 y una inspección
  exhaustiva de helpers externos no se han verificado. No ejecutar copias de nsc
  por rutas no autorizadas, ni baterías completas, push, merge o despliegue.
