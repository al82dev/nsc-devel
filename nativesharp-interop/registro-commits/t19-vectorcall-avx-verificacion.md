# Tanda 19: agregados `__vectorcall` de vectores de 256 y 512 bits (T19-036), verificación

## Qué se probó

Se preparó un cambio del backend que acepta, para `__vectorcall`, agregados de vectores de Clang de 256 y 512 bits,
además de los de 128 bits:

- `vectorCallAggregate` cuenta campos vectoriales de 16, 32 o 64 bytes;
- `vectorCallAbiType` da `<8 x float>` o `<16 x float>` por miembro;
- `vectorCallPlanAgrees` acepta cualquier vector del ancho del miembro.

Se probó con el caso `vectorcall-avx`: `W8 {f8 a, b}`, `D4 {d4 v[3]}`, `Z1 {i16 z}` y una función con un
agregado que ya no cabe en los registros.

## Resultado

- En estática y en DLL, O0–O3, directo y desde plantilla precompilada, NativeSharp y Clang devuelven lo mismo, y
  las 7 declaraciones de NSC tienen la forma de las de Clang.
- Pero ese resultado común no es correcto. Un programa solo de Clang, con la biblioteca y `main` compilados por
  Clang 23.1.0 con el destino x86-64 base (`+sse,+sse2`), imprime `d4sum` = 20971538 donde el código fuente da 97,
  y `wmix` = 896 donde da 816. El control de Clang pierde los valores al pasar vectores de 256/512 bits sin AVX.
- Sin AVX, la convención no es coherente ni entre dos unidades de Clang. Coincidir con Clang en ese destino no
  demuestra que la capacidad funcione.

## Por qué no se aplicó

- NativeSharp compila los proyectos solo con el nivel de CPU base. `TargetInfo` tiene `x86-64-v2/v3/v4`, pero
  ninguna acción del driver ni del proyecto lo selecciona: todas usan `LlvmTargetMachine::createWindowsX64(level)`.
- Una biblioteca compilada con AVX pasa esos vectores en registros YMM/ZMM, y NativeSharp no puede emitir esa
  llamada.
- La capacidad necesita antes un nivel de CPU por proyecto, que valga también para la sesión de Clang y para la
  identidad de los módulos precompilados. Después hay que comparar con la biblioteca compilada con el mismo
  `-march`.
- El cambio no se aplicó: el plan de Clang sigue rechazando esas declaraciones con diagnóstico. El cambio y el
  caso quedan en `build-claude-t08/tools/t19c/av1` y `av2-traslados.py` para cuando exista el nivel de CPU.

## Sigue pendiente (T19-036)

- Agregados `__vectorcall` de vectores de 256 y 512 bits, con un nivel de CPU AVX por proyecto.
- Vector de Clang suelto como parámetro o resultado (`__m128 f(__m128)`).
  - Los tipos de `NativeSharp.Core.Numerics` (`Float32x4`, `Int32x4`…) son structs ordinarios fuera de
    `__vectorcall`.
  - En el ABI de Microsoft x64, un `__m128` se devuelve en XMM0 y se pasa por dirección alineada a 16. Un struct de
    16 bytes, en cambio, se devuelve por puntero oculto y tiene alineación 4.
- Punteros a función C++ `__vectorcall` con records por valor.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
