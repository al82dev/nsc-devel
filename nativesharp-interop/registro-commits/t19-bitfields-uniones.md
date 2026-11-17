# T19-004: bit-fields de tipo enum o de más de 64 bits, `__int128` y campos de `union` C++

Fixture nuevo, con y sin `--dll`: `pruebas-tanda-19-bitfields-uniones/validar.py`, para C y C++.

Sonda de desarrollo: `tools/t19c/d15/sonda-t19-004.py` (`logs/sonda4.log`).

## Defectos

Confirmados con la sonda (`logs/sonda4.log`, antes del cambio):

- Un bit-field de tipo enum no se importaba («has no compatible member»), en C ni en C++.
  - Afectaba a `enum Color c : 2` y a `enum class E : unsigned char e : 2`.
- `__int128` y `unsigned __int128` no tenían portador.
  - No se importaba ningún campo, global ni función que los usara («C/C++ return type is not yet mapped»).
  - Tampoco un bit-field de más de 64 bits (`unsigned __int128 big : 100`).
- Una `union` C++ con un miembro record, aunque fuera trivial, no importaba ningún miembro.
- Una union anónima dentro de una clase C++ tampoco importaba sus miembros; en C sí funcionaba.

## Decisiones

- **Bit-field enum**:
  - se lee y escribe por el tipo entero subyacente del enum, con el signo que Clang aplica al layout;
  - el campo conserva el tipo enum;
  - en el ABI de Microsoft un enum C es `int`, así que `BLUE` (2) en `enum Color c : 2` se lee -2, como en Clang.
- **`__int128`** es `i128` y `unsigned __int128` es `u128`, de 16 bytes alineados a 16:
  - en campos y bit-fields;
  - en parámetros y resultados, con el plan ABI de Clang. Windows x64 pasa el valor por la dirección de una copia
    alineada a 16 y lo devuelve en XMM0 como `<2 x i64>`. LLVM, con un `i128`, usaría RCX:RDX y RAX:RDX.
- **Bit-field de más de 64 bits**: la ventana de acceso llega a 16 bytes. Las máscaras de 128 bits se construyen
  con constantes de precisión arbitraria.
- **Union C++**:
  - una union que Clang encuentra trivialmente copiable, con constructor por defecto y destructor triviales, solo
    tiene miembros triviales, records incluidos, así que tiene la semántica de una union C;
  - en un record C++, una union o un struct sin nombre trivial viaja como el portador anónimo de C (T08-D14).

## Diseño

- `CppSemanticRecord.cpp`:
  - en la proyección de un bit-field, el tipo enum (`projectCppSemanticEnumType`) y las ventanas de 16 bytes;
  - `cppUnionHasOnlyScalarMembers` acepta una union trivial;
  - la materialización de portadores anónimos y el campo portador valen también para un propietario C++;
  - la reconstrucción desde el catálogo restituye los rasgos de valor de Clang (`valueTraits`).
- `ClangSession.cpp`:
  - `cppUnnamedRecordsAreTrivial` decide si un record C++ lleva portadores anónimos;
  - una función C++ con un entero de 128 bits obtiene el plan de Clang, como una con un vector.
- `CppScalarTypeMapping.cpp`: los enteros de 128 bits.
- `CppSemanticInventory.cpp`: una función C con un entero de 128 bits obtiene el plan de Clang
  (`attachCVectorPlan`).
- `CppCRecordMetadataCodec.cpp`: ese plan viaja en el catálogo precompilado.
- `OopLlvmTypesAbiAttributes.cpp` (`cVectorPlanValues`) y `OopLlvmCalls.cpp`: la declaración y la llamada de la
  importación siguen el plan, con la copia alineada y la conversión desde `<2 x i64>`.
- `OopLlvmFieldsAndConversions.cpp`, `ProjectMemberModel.cpp` y `ProjectObjectLayoutModel.cpp`: unidades de
  bit-field de hasta 16 bytes.
- `CppSemanticFunction.cpp` (puntero a función) y `ConversionAndPassKindChecks.cpp` (argumento variádico): los
  rechazos de T19-044.

No se modificó Clang ni NSLink. El contrato NSMeta no cambia: los bit-fields, los tipos de 128 bits y los planes
ya tenían su codificación.

## Qué funciona, como en Clang

- **C**:
  - bit-fields de enums con y sin signo;
  - bit-fields `unsigned __int128 : 100` y `__int128 : 70` con signo;
  - campos `unsigned __int128`;
  - funciones que toman y devuelven `__int128` y `unsigned __int128`;
  - union anónima en un struct.
- **C++**:
  - bit-fields de `enum class` y de un enum con base `signed char`;
  - union con un miembro record trivial, leída por cualquiera de sus miembros;
  - union anónima en una clase;
  - bit-field `unsigned __int128 : 100`;
  - una función C++ con `unsigned __int128`.

Todo O0–O3, en biblioteca estática y DLL, directo y desde una plantilla precompilada con y sin encabezado.

En la parte C++, los objetos vienen de funciones de la biblioteca (`makeS`, `makeU`…). El cuerpo de una plantilla
precompilada todavía no crea un record C++ con `new`, límite que sigue en T19-016.

## Evidencia

| Fixture | Resultado | Estática | DLL |
|---|---|---|---|
| `pruebas-tanda-19-bitfields-uniones` (C) | 4294309703 | `A:/temp/nsc-t19-bitfields-3hep9136` | `A:/temp/nsc-t19-bitfields-z1lxf_gs` |
| `pruebas-tanda-19-bitfields-uniones` (C++) | 4294095838 | `A:/temp/nsc-t19-bitfields-3hep9136` | `A:/temp/nsc-t19-bitfields-z1lxf_gs` |

Rechazo genuino, también en Clang: asignar un entero a un bit-field de un enum con ámbito («cannot implicitly
convert assignment RHS from 'builtin:int'», y en Clang «assigning to 'E' from incompatible type 'int'»).

Validación de este cambio, con el binario final:

- Tandas 10 a 19 en estática: 77 de 77 (`logs/regresion-t1*-d58.log`).
- Tanda 9 en estática: pasan 1094 de 1099 y se omiten 2 que exigen parámetros (`logs/regresion-t09-d58.log`).
  - Fallan, como en la línea base, los dos `cpp-variadica-preserve-none` (bloqueo de Clang, T19-032).
  - Bajo carga falla `cpp-tamano-objeto/validar-indice-expresion` (T19-026).
- `ninja -k 0`, los nueve CTest de interop y la identidad de NSC pasan (`logs/lote-ct2.log`).
- El fixture nuevo pasa en DLL (`logs/fx58d.log`).
- Una regresión anterior con el mismo cambio, antes del ajuste de T19-044 (`d56`), pasó completa:
  - DLL de las tandas 9 a 19: 76 de 76;
  - los tres guiones de T08 terminan con 0.

La regresión DLL completa y los guiones de T08 con el binario final quedan para la regresión global, por decisión
del usuario («no hagas regresiones completas, haz correcciones puntuales, cuando termines todo, haces la
regresion global»).

Una corrida intermedia (`d57`) detectó que el primer rechazo de T19-044, en `describeType`, cambiaba el diagnóstico
de `pruebas-tanda-09-sysv-indirecta`. El rechazo pasó a la proyección del puntero a función, y el fixture volvió a
pasar con su mensaje original.

## Límites

Con tarea nueva:

- T19-043: unions C++ con miembros de vida no trivial, también anónimas. Siguen sin importar sus miembros, porque
  requieren construir y destruir el miembro activo.
- T19-044: `__int128` en punteros a función, callbacks y llamadas variádicas, y bit-fields `volatile` de más de
  64 bits. Se rechazan con diagnóstico, porque esas llamadas usarían el paso `i128` de LLVM y no el de Clang:
  - un puntero a función con un entero de 128 bits no se mapea, así que la función que lo toma o devuelve no se
    importa («C/C++ return type is not yet mapped»). Un callback con uno conserva su rechazo anterior («C callback
    signature contains an unavailable type», `pruebas-tanda-09-sysv-indirecta`);
  - un `u128` como argumento variádico C no es viable.
  - Ambos casos se comprobaron antes de este rechazo, y fallaban en ejecución con 0xC0000005
    (`A:/temp/t19-004-fp`, `A:/temp/t19-004-va`). El variádico ya fallaba antes de T19-004.
