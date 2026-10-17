# Tanda NSC 3.0.5 L06E - inline transportado alcanzable

## SCOPE LOCK

- Objetivo exacto: impedir que un consumidor compile cuerpos `inline`/`forceinline` transportados
  por metadata cuando ninguna definicion local alcanzable los referencia.
- Naturaleza: optimizacion correctiva del proceso cold LLVM.
- Revision correcta de partida: `4b898ab9d6e068d933935ff32d87fd902a1f2a3b`.
- Entrada exacta: programa minimo con una sola funcion local y un catalogo precompilado amplio.
- Evidencia: `A:\temp\nsc305-l06-largecatalog\run-before-flat`; el programa produjo ocho code
  units y `llvm-object-generation=3.890.329 us`. Cada unidad ordinaria gasto 2,33-2,64 s en
  lowering aun cuando el consumidor no llama esa superficie.
- Operacion responsable: `ProjectMemberModel` convierte el cuerpo inline transportado en entrada
  ejecutable local; `ProjectOopCodePartition::build` considera toda entrada no importada como raiz,
  sin distinguir procedencia ni alcanzabilidad.
- Invariante: un cuerpo transportado existe para satisfacer una llamada concreta del consumidor;
  no es una raiz de emision por el solo hecho de estar disponible.
- Archivos/simbolos autorizados:
  - `ClassMemberSymbol`, `ProjectOopHirFunction`, `ProjectOopMirFunction`: bit interno de procedencia;
  - propagacion Member -> HIR -> MIR, dump y serializacion/huella interna MIR;
  - nuevo helper LLVM interno de alcanzabilidad y su alta en CMake;
  - `ProjectOopCodePartition::build` y `persistentPlanKey`: consumir la clausura iterativa;
  - `ProjectOopObjectGenerator::generate` y `OopLlvmImplementation::declareFunctions`: poda de
    declaraciones transportadas no alcanzables en el coordinador;
  - `CrossLibraryInliningTests.cpp`: rojo permanente y paridad funcional existente;
  - este expediente y tracking al cerrar.
- Protegido: formato NSMETA, ABI, nombres fisicos, politicas inline/noinline, metadata publicada,
  cuerpos transportados alcanzables, diagnosticos, formato COFF y comportamiento del productor.
- Algoritmo autorizado: marcar procedencia sin cambiar metadata; iniciar la worklist con todas las
  definiciones locales reales; recorrer MIR iterativamente; incorporar un cuerpo transportado sólo
  cuando un callee/member de la clausura lo referencia; repetir hasta punto fijo determinista.
- Prohibido: eliminar inline transportado, cambiar su semantica, introducir versionado NSMETA,
  podar definiciones fuente, usar recursión, ejecutar baseline/bateria completa o usar otra unidad.
- Prueba roja: productor con doce inline transportados no usados; consumidor que llama sólo la
  superficie original. Antes de la correccion, su IR contiene las doce definiciones `weak_odr`.
- Criterio de terminacion de esta hipotesis: la IR del consumidor no contiene esas definiciones,
  conserva los cuatro calls y politicas verificadas, enlaza y ejecuta con exit 0. La medicion del
  programa minimo amplio decide, por separado, si esta causa explica el costo dominante de L06.

## Resultado

- Se agrego procedencia interna `isTransportedInlineBody` y se propago Member -> HIR -> MIR sin
  cambiar NSMETA, ABI ni nombres fisicos.
- La clausura se calcula mediante una worklist iterativa. Las definiciones fuente siguen siendo
  raices; un cuerpo transportado entra solamente cuando una referencia MIR alcanzable lo exige.
- El rojo exacto demostro que el consumidor emitia `Unused01` a `Unused12` como definiciones
  `weak_odr` aunque no las llamaba.
- Despues de la correccion, `nsc_cross_library_inlining_tests` paso `16/16`: las definiciones no
  usadas desaparecen, las llamadas transportadas necesarias conservan su politica, el consumidor
  enlaza y termina con exit 0.
- `nsc_project_oop_lowering_tests` paso `101/101` y `nsc.exe` se reconstruyo correctamente.

## Contrafactual de rendimiento

Cinco ejecuciones cold del programa minimo con catalogo precompilado amplio produjeron una mediana
LLVM de `3.843.405 us`, frente a `3.890.329 us` antes del cambio. La diferencia no es material y
las nueve unidades fisicas permanecieron. Por tanto:

- L06E cierra un trabajo incorrecto real y conserva una regresion permanente;
- L06E no se declara causa principal del tiempo cold;
- L06 permanece abierta;
- la evidencia siguiente debe investigar el trabajo comun repetido por unidad, en particular el
  catalogo importado de funciones, campos estaticos y metadata de despacho.

Evidencia en disco:

- rojo: `A:\temp\nsc305-l06e-red-final`;
- cinco cold posteriores: `A:\temp\nsc305-l06-largecatalog\run-after-l06e`;
- MIR del catalogo: `A:\temp\nsc305-l06-largecatalog\mir-audit\mir.log`.
