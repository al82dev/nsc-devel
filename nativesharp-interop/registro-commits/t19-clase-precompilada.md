# Tanda 19: records `class` en el catálogo precompilado (T19-036, T11-002)

## Defecto

Una plantilla NativeSharp precompilada que usa un record C++ declarado con `class` no se publica. El productor
falla al verificar su metadata: `record is not a consistent C declaration`. El camino directo pasaba; el cierre de
la tanda 11 había trasladado el caso a T11-002 sin evidencia propia.

## Causa

El catálogo de records precompilado solo admitía `struct` y `union`:

- `captureCppCRecordSnapshot` (`CppSemanticRecord.cpp`) rechazaba toda entrada de otro tipo;
- `encodeCRecordCatalog` y `decodeCRecordCatalog` (`CppCRecordMetadataCodec.cpp`) también;
- la comprobación de instalación del consumidor, igual;
- la proyección de bit-fields de un record C++ dejaba fuera `class`.

Una `class` C++ con miembros públicos tiene el layout y el ABI de valor de un `struct`. El símbolo de cada función
sale de Clang, así que la diferencia de decoración de MSVC (`V` frente a `U`) no se reconstruye.

## Cambio

- `CppSemanticRecord.cpp`: el snapshot, la comprobación de instalación y los bit-fields admiten `class` en un record
  C++.
- `CppCRecordMetadataCodec.cpp`: el catálogo escribe y lee `class` como el byte de tipo que ya tiene `CppRecordKind`.
  Solo lo acepta con la observación de valor C++ (`cppValueObservation`); una `class` sin ella se rechaza. Un
  catálogo sin records `class` conserva sus bytes.

El replay C sin encabezado de `sysv_abi` variádico sigue exigiendo `struct`: reproduce records C, y una `class` no
llega por ese camino.

## Evidencia

Caso `clase-precompilada` de `pruebas-tanda-19-traslados`:

- `class Pt` por valor como parámetro y resultado de funciones de la biblioteca;
- un campo modificado desde NativeSharp;
- `class Counter` con un método estático inline.

Resultado: 42024 igual que Clang, O0–O3, en estática (`logs/fx-cl3.log`) y en DLL (`logs/fx-cl3d.log`). Pasa
directo y desde una plantilla precompilada, con y sin encabezado. Las 2 declaraciones del IR de NSC tienen la forma
de las de Clang.

Regresión dirigida: ver TRACKING.

## Verificado al probar, fuera de este cambio

- Un método de instancia C++ ordinario, inline o fuera de línea, llamado sobre un local dentro de una plantilla
  precompilada falla en el consumidor: `has no compatible member named 'value'`. La tanda 11 lo había dejado fuera
  (`pruebas-tanda-11-metodos`: «los métodos de instancia aún no se transportan en una plantilla precompilada»). Queda
  en T19-031, flujos de receptor en plantillas precompiladas. Por eso el caso usa un método estático.
- Las globales C++ de namespace y los campos estáticos usados desde una plantilla precompilada fallan en el
  consumidor: `unknown identifier 'T'`. El catálogo solo transporta globales C. Es el siguiente traslado de T19-036,
  en `casos_pendientes.py` (`globales-precompilada`).

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
