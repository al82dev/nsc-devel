# Tanda 19: agregados de vectores de 128 bits con `__vectorcall` (T19-036)

Continúa [records C++ con `__vectorcall`](t19-vectorcall-records-cpp.md), que dejó pendientes los agregados de
vectores.

## Defecto

Un record C++ cuyos campos son vectores de Clang de 128 bits (`float` x4, `double` x2, `int` x4; los tipos de
`__m128`, `__m128d` y `__m128i`) no se pasaba como Clang con `__vectorcall`. NativeSharp ve cada campo vector como
un arreglo de sus carriles: la vista de vector de T14, con `ClassMemberSymbol::cppArrayPhysicalLayouts`.
`vectorCallAggregate` contaba entonces los carriles como miembros: `{f4 a; f4 b;}` tenía 8 `float`. Con más de cuatro
miembros, el record no era un agregado homogéneo y pasaba como un record ordinario. Clang lo trata como un agregado
homogéneo de dos vectores (`isX86VectorTypeForVectorCall`). La comprobación del plan de Clang rechazaba la
importación.

## Cambio

- `OopLlvmTypesAbiAttributes.cpp`, `vectorCallAggregate`. Un campo C++ con vista de vector cuyo componente interior
  mide 16 bytes con alineación 16 cuenta un miembro vectorial de 16 bytes por vector, solo o en un arreglo. Con
  vectores más anchos o de otra alineación, el record no se clasifica como agregado; la comprobación del plan lo
  rechaza si Clang lo clasifica distinto.
- `OopLlvmDeclarations.cpp`, `vectorCallPlanAgrees`:
  - el miembro de un agregado vectorial es cualquier vector de 128 bits del plan (`<4 x float>`, `<2 x double>`,
    `<4 x i32>`);
  - el tipo del plan se recorre por sus estructuras y arreglos anidados, como lo conserva Clang
    (`{ [2 x <4 x i32>] }`), y se cuentan sus hojas.

La vista que ve NativeSharp no cambia. Cada vector viaja en un registro XMM y se convierte por memoria entre el
portador de NativeSharp y el tipo del ABI, como los agregados de `float` y `double` de T08-D22.

## Evidencia

Caso `vectorcall-vectores` de `pruebas-tanda-19-traslados`. Firmas:

- `W2 {f4 a, b}` y `D3 {d2 a, b, c}` como resultado y como parámetro;
- `I2 {i4 v[2]}`, un arreglo de vectores, como resultado;
- `isum(I2, W2, W2, double)`: el `double` toma un registro; `I2` y el primer `W2` toman cuatro; el segundo `W2` ya no
  cabe y va por dirección.

El resultado es 1368210 en NativeSharp y en Clang, O0–O3. Pasa en estática (`logs/fx-vv3.log`) y en DLL
(`logs/fx-vv3d.log`), directo y desde una plantilla precompilada, con y sin encabezado. En las 6 declaraciones, el
IR de NSC tiene la forma de Clang: `inreg` en los agregados que viajan en registros y puntero en el que no cabe.

Regresión dirigida:

- `lista-vv4.txt` (convenciones, matriz de firmas, `vectorcall` C de la tanda 9 y `pruebas-tanda-14-especiales`):
  4 de 4 en estática y 1 de 1 en DLL (`logs/regresion-vv4`, `logs/regresion-dll-vv4d`);
- el fixture completo: 6 casos en estática y 5 en DLL, O0–O3 (`logs/fx-vv4.log`, `logs/fx-vv5d.log`);
- la copia local de `vectorcall` C de la tanda 8 (`logs/t08-vectorcall-vc8.log`).

## Sigue pendiente

- Vectores de 256 y 512 bits (`__m256`, `__m512`). Ocupan registros AVX, y no lo he verificado con este destino. El
  plan de Clang rechaza la importación.
- Un vector suelto como parámetro o resultado (`__m128 f(__m128)`): NativeSharp no proyecta tipos vector de
  C/C++ fuera de un record.
- Punteros a función C++ `__vectorcall` con records por valor.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
