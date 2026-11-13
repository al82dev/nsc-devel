# Campo de enum C++ global en record trivial

El record C++ trivial con un campo `Mode` no exponía ese campo a NativeSharp,
aunque ya se importaban el enum global no scoped con base `int` y sus
enumeradores. La reproducción inicial está en
`A:/temp/t10-campo-enum-sonda/resultado.log`. Con una declaración explícita de
`Mode` el campo pasaba, lo que mostró además que faltaba seleccionar el enum
como dependencia del record.

La proyección selecciona el enum global cuando se necesita para un campo
directo de un record C++ trivial, verificando USR, tipo y representación. La
proyección del campo conserva su identidad enum; no se habilitan campos
`volatile`, arreglos ni enums con namespace por este cambio.

`pruebas-tanda-10-enum-cpp-global/validar.py` ejecuta lectura y escritura de
un valor negativo y un campo entero adyacente, tanto mencionando el tipo como
sin mencionarlo, en O0–O3. El control C++ de Clang y la regresión C de O0
pasaron, igual que los rechazos de identidad y de las formas todavía no
implementadas. Registros: `A:/temp/nsc-t10-enum-cpp-global-pw081o75`;
compilación incremental código 0.

No se acredita paso del record por copia, consumo precompilado, enum en
namespace ni el resto de campos de T11-004. T09-003 y T11-004 siguen abiertos.
