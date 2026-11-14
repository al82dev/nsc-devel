# T09: tamaño oculto de records importados

Una llamada a una función C o C++ con `pass_object_size(1)` o
`pass_object_size(3)` rechazaba la dirección de un record local o un campo
record, aunque Clang conociera su tamaño. NSC ahora usa el tamaño del layout
importado y completo para structs y unions C y records C++ de layout estándar
y copia trivial. Los tipos incompletos o sin layout siguen rechazándose.
El cálculo distingue el tamaño del subobjeto del objeto completo que lo
contiene; no modifica el contrato de NSMeta.

## Validación

- Compilación incremental de `nsc`, incluida la comprobación del runtime del host.
- `pruebas-tanda-09-cpp-tamano-objeto/validar-subobjeto-record.py`: records
  C++ locales y campos, con y sin relleno, cuatro modos estáticos y dinámicos,
  convenciones predeterminada y SysV, biblioteca estática y DLL, O0–O3.
  Cada convención contrasta ocho ejecuciones C++/NativeSharp y dieciséis
  consumidores precompilados con y sin encabezado. Evidencia:
  `A:/temp/nsc-t09-size-record-dmwi4v7z` y
  `A:/temp/nsc-t09-size-record-dpp6h7qi`.
- `pruebas-tanda-09-c-tamano-objeto/validar-subobjeto-record.py`: struct C
  con relleno y union C, locales y campos, en los mismos modos, convenciones y
  configuraciones; ocho comparaciones y dieciséis consumidores precompilados
  por convención. Evidencia: `A:/temp/nsc-t09-c-size-record-5srbt870` y
  `A:/temp/nsc-t09-c-size-record-9qoc9kqo`.
- Regresión de los tamaños escalares C, estática y DLL O0–O3:
  `A:/temp/nsc-t09-c-object-size-pywh2acp`.

## Límites

Estas pruebas cubren las formas indicadas en Windows x64. No acreditan otros
layouts C++, arreglos de records, expresiones arbitrarias, métodos ni todas las
convenciones. T09-003 permanece abierto.
