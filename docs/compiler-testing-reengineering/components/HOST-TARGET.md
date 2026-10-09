# Host.Target — Identidad de plataforma y reglas de layout Windows

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 4 archivos, 781 líneas y 26,937 bytes.
- **Inventario:** 46 símbolos físicos —13 tipos y 33 funciones—, 30 contratos y 240 casos diseñados.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura, consulta a LLVM o modificación del producto.

## 2. Responsabilidad y frontera

Host.Target concentra dos responsabilidades estables:

1. `TargetDataLayout`: tamaños, alineaciones, stride, arrays y agregados compatibles con Windows LLP64;
2. `TargetInfo`: arquitectura, formato, ABI, triple LLVM, nivel CPU y vocabulario de assembly aceptado.

No clasifica argumentos de calling convention ni emite código. `TargetDataLayout::WindowsArm64` declara que los
layouts escalares, de puntero y agregados aquí modelados son iguales a x64; las diferencias ABI posteriores
pertenecen al backend.

## 3. Modelo de datos

`BuiltinLayoutKind` distingue bool, enteros con y sin signo, enteros nativos y de 128 bits, floating, enum y
puntero. Esta capa sólo modela tamaño y alineación: por eso `SByte` y `Byte` comparten layout.

Los resultados son valores simples:

- `TypeLayout`: tamaño y alineación;
- `ArrayLayout`: tamaño, alineación, stride y count;
- `StructFieldLayout`: offset, tamaño, alineación y metadata bit-field opcional;
- `StructLayout`: tamaño, alineación y vector ordenado de campos.

`StructLayoutOptions` selecciona union, pack y alineación mínima agregada. `StructFieldRequest` añade offset
explícito, alineación mínima local, flexible array, bit width y marca de bit-field anónimo.

## 4. Layouts builtin y propiedades de plataforma

La tabla vigente es:

| Tamaño/alineación | Kinds |
|---|---|
| 1/1 | Bool, SByte, Byte, Char |
| 2/2 | Short, UShort |
| 4/4 | Int, UInt, Float, Enum |
| 8/8 | Long, ULong, NInt, NUInt, Double, Pointer |
| 16/16 | Int128, UInt128 |

Un enum fuera de tabla produce `{0,1}`. Ambas fábricas de Windows devuelven el mismo objeto sin estado. Las
propiedades publicadas son little-endian y `maximumNaturalAlignment() == 8`; el layout explícito de enteros de
128 bits sigue siendo 16/16 y el algoritmo de struct usa la alineación declarada del campo sin aplicar ese getter
como cap.

## 5. Aritmética comprobada, stride y arrays

`checkedAdd` y `checkedMultiply` rechazan overflow de `uint64_t`. `alignUp` exige alineación potencia de dos,
suma la máscara con `checkedAdd` y borra bits bajos. `strideOf` rechaza tamaño o alineación cero y alinea el tamaño.

Un array exige count no cero, stride válido y producto representable. El resultado conserva la alineación del
elemento; tamaño es `stride * count`.

## 6. Agregados

### 6.1 Struct simple

El overload de `TypeLayout` coloca cada campo en `alignUp(currentOffset, field.alignment)`, avanza por tamaño,
mantiene la máxima alineación y redondea el tamaño final. Un struct vacío vale tamaño 1, alineación 1.

### 6.2 Forma general

El overload de requests valida que `pack` y `minimumAlignment` agregada sean cero o potencias de dos. Para cada
campo calcula alineación efectiva como:

```text
pack == 0 ? natural : min(natural, pack)
effective = max(effective, field.minimumAlignment) cuando existe
```

Un agregado vacío inicia con extent 1 y se redondea a su mínimo agregado; por eso una alineación mínima 16 produce
tamaño 16. Para un campo normal:

- offset explícito se acepta y puede solapar;
- offset implícito de struct se alinea desde el cursor;
- offset implícito de union es cero;
- sólo un campo con `minimumAlignment` explícito exige que su offset explícito sea múltiplo de la alineación
  efectiva;
- extent es el máximo de `offset + size` y el tamaño final se redondea a la alineación agregada.

### 6.3 Pack, union y flexible arrays

Pack limita la alineación efectiva de cada campo. Union no avanza cursor y su extent es el mayor final de miembro.
Un flexible array puede tener tamaño cero, conserva alineación y puede usar las mismas reglas de offset; el código
vigente no comprueba que sea el último campo. No puede combinarse con `bitWidth`.

## 7. Bit-fields MSVC

Un bit-field no admite offset explícito, mínimo local ni flexible array. El allocation unit es el `TypeLayout` del
campo. Se rechaza overflow al convertir bytes a bits, width mayor a la unidad y width cero si no es anónimo.

En struct secuencial, campos con el mismo tamaño de unidad comparten unidad mientras quepan. Cambiar tamaño o
exceder bits restantes abre una unidad alineada nueva. Cada resultado conserva el offset de unidad, tamaño de
unidad, alineación efectiva, bit offset acumulado y width.

Un bit-field anónimo de width cero:

- antes de cualquier unidad activa queda en offset cero, tamaño cero y no eleva la alineación agregada;
- después de una unidad activa alinea el cursor al tipo base, actualiza extent/alineación y cierra la unidad.

En union, cada bit-field no cero ocupa una unidad completa en offset y bit offset cero. Contribuye al extent pero,
por el `continue` vigente, no contribuye a la alineación natural de la union; sólo el mínimo agregado puede elevarla.

## 8. Identidad de target y CPU

Las fábricas producen:

| Target | Arquitectura | OS | Formato | Triple | ABI |
|---|---|---|---|---|---|
| Windows x64 | X86_64 | Windows | COFF | `x86_64-pc-windows-msvc` | `Microsoft Windows x64 ABI` |
| Windows ARM64 | Arm64 | Windows | COFF | `aarch64-pc-windows-msvc` | `Microsoft Windows ARM64 ABI` |

La fábrica x64 con preferred/available selecciona el menor ordinal de ambos. Conserva preferred como solicitado
y el seleccionado como efectivo; `usedCpuFeatureFallback` compara ambos. La fábrica por defecto usa Baseline.

ARM64 publica baseline `armv8-a`, CPU `generic` y features `+neon,+fp-armv8`. x64 publica baselines `x86-64-v1`
a `v4`; Baseline usa CPU `x86-64` y los demás el nombre del baseline. Las feature strings son tablas exactas por
nivel, no una unión textual acumulada.

## 9. Consulta de features

ARM64 admite feature vacía, `neon` y `fp-armv8`. x64 siempre admite vacía, `x87`, `mmx`, `sse` y `sse2`; cada
nivel posterior admite además sus conjuntos anteriores:

- v2: sse3, ssse3, sse4.1, sse4.2, popcnt;
- v3: avx, avx2, bmi, bmi2, f16c, fma, lzcnt, movbe, xsave;
- v4: avx512f, avx512bw, avx512cd, avx512dq, avx512vl.

Los nombres son case-sensitive. Features no enumeradas devuelven false, incluso si
`requiredAssemblyFeature` puede nombrarlas como requisito de un mnemónico.

## 10. Assembly

### 10.1 Registros

ARM64 reconoce `sp`, `fp`, `lr`, `xzr`, `wzr`, `x0..x30`, `w0..w30` y `v0..v31`. El ordinal se parsea completo;
texto sobrante o ausencia de dígitos falla.

x64 reconoce aliases completos de rax..rsp, `r8..r15` con sufijos `d/w/b`, `xmm0..15`, `mm0..7` y `st0..7`.
Los nombres son minúsculos y el parser exige consumo total. Los `r0..r7` numéricos se rechazan; sus registros
históricos sólo existen mediante aliases.

### 10.2 Requisito por mnemónico

ARM64 devuelve siempre vacío. En x64:

- prefijo `v` → avx;
- prefijo `aes` → aes;
- prefijo `pclmul` → pclmul;
- `popcnt` y `lzcnt` → homónimo;
- `crc32`, `pcmpestr*`, `pcmpistr*` → sse4.2;
- la tabla BMI → bmi;
- cualquier otro → vacío.

La clasificación es puramente textual y case-sensitive.

## 11. Complejidad y estrategia de prueba

Queries builtin, target y feature son constantes salvo lookup/hash o parse corto. Arrays son constantes. Layout de
struct es `O(F)` tiempo y `O(F)` memoria. Registro/mnemónico son constantes respecto al tamaño acotado del nombre.
No hay recursión.

Los 30 contratos del test plan producen 240 casos: 210 `PR` y 30 `NIGHTLY`. Se diseñan oracles independientes
con aritmética de precisión suficiente para detectar overflow, matrices completas de opciones/layouts y tablas
exhaustivas de CPU, features, registros y mnemónicos. LLVM no es necesario para validar estos contratos puros.

## 12. Reconstructibilidad

Los 4 archivos, 46 símbolos, 30 contratos y 240 casos permiten reconstruir tablas de plataforma, aritmética,
layouts simples y generales, bit-fields y consultas de target/assembly. La segunda pasada está en
`audits/TANDA-08-HOST-TARGET-AUDIT.md`.

**Resultado:** `RECONSTRUIBLE` para la revisión congelada, sin afirmar ejecución ni cobertura.
