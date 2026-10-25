# NSC — Diseño del sidecar ThinLTO de bibliotecas estáticas

**Fecha:** 2026-10-25
**Rama:** `development-fixes-debug-ide`
**Plan:** optimización parte 2, punto 47 (diseño) y punto 48 (consumo)
**Antecedente:** `NSC-AUDITORIA-LTO-2026-10-24.md`, sección 8, LTO-03

## 1. Objetivo

Una biblioteca estática construida con `-flto=thin` publica, junto a su `.lib`, el bitcode de pre-enlace de cada
unidad de código. Un ejecutable o una DLL construidos con `-flto=thin` pueden entonces importar e insertar en línea
funciones de esa biblioteca durante sus backends ThinLTO, en lugar de llamar a los objetos nativos ya cerrados.

El sidecar es una aceleración opcional. El contrato de biblioteca de tres artefactos (`.lib`, `.nsmeta` y runtime
opcional) no cambia, NSMETA no se versiona y el objeto nativo sigue siendo siempre un respaldo válido.

## 2. Alcance

| Caso | Decisión |
|---|---|
| Productor: biblioteca estática con ThinLTO efectivo | Publica `<Biblioteca>.nsthinlto`. |
| Productor: DLL | No publica sidecar: el límite de la DLL es un contrato binario y no debe importar cuerpos. |
| Consumidor: ejecutable o DLL con ThinLTO efectivo | Consume cada sidecar compatible de sus referencias estáticas. |
| Consumidor: biblioteca estática | No consume: su `.lib` lleva sólo su código; el enlace final ve cada referencia. |
| Referencias a DLL (`runtimePath` no vacío) | No se consideran. |
| Paquetes restaurados (`nsc restore`) | v1 no copia el sidecar; el consumidor usa la `.lib` nativa. |
| `-O0`, `-flto=none`, `-flto=full`, host DYNAMIC | No hay ThinLTO efectivo: ni se produce ni se consume. |

ThinLTO efectivo significa `-flto=thin`, nivel `-O1` a `-O3` y host STATIC (`ThinLtoBackend::available()`).

## 3. Ubicación

El sidecar vive junto a la biblioteca: `Biblioteca.lib` → `Biblioteca.nsthinlto` (`replace_extension`). Se publica
en la misma transacción de artefactos que la `.lib`, el `.h`, la `.nsmeta` y el `.nsproducts`. Cuando una
construcción posterior de la misma biblioteca no tiene ThinLTO efectivo, el sidecar publicado antes se elimina
después del commit; aunque quedara, su enlace criptográfico con la `.lib` lo rechazaría.

Cuando la biblioteca lo produce, el sidecar es una salida requerida del bundle de compilación: restaurar el bundle
exige que exista y que su digest coincida.

## 4. Formato v1

Todos los enteros son little-endian. Una cadena es `u32 longitud` seguido de esa cantidad de bytes UTF-8.

| Campo | Contenido |
|---|---|
| Firma | 8 bytes `NSTHINLT`. |
| Versión | `u32` = 1. |
| Compatibilidad | Cadena: identidad de compatibilidad (sección 5). |
| Digest de biblioteca | Cadena: SHA-256 hexadecimal de los bytes de la `.lib` publicada. |
| Digest de metadata | Cadena: SHA-256 hexadecimal de los bytes de la `.nsmeta` publicada. |
| Unidades | `u32` número de unidades, luego por unidad `u64` tamaño y el bitcode. |
| Integridad | 32 bytes: SHA-256 de todos los bytes anteriores. |

Cada unidad es exactamente el bitcode que el productor entregó a sus propios backends: el módulo después del
pipeline `thinlto-pre-link<On>`, con su resumen de módulo y su hash de módulo (`GenerateHash`). El orden de las
unidades es el ordinal de partición del productor.

El lector rechaza: firma o versión distintas, longitudes que exceden el archivo, más de 65 536 unidades, unidades
vacías, bytes sobrantes y una integridad que no coincide.

## 5. Identidad de compatibilidad

Texto de líneas `clave valor`, comparado byte a byte:

```
llvm <LLVM_VERSION_STRING>
triple <triple del target>
cpu <CPU del target>
features <cadena de features>
layout <data layout>
optimization O<n>
debug <none|line-tables-only|full>
codegen <kCodegenCacheIdentity>
policy <codeGenerationPolicyIdentity(thin)>
```

La versión exacta de LLVM, el target (triple, CPU, features y data layout), el nivel de optimización, el tipo de
información de depuración, la identidad de codegen de este compilador y la política de generación de código deben
coincidir. Cualquier diferencia rechaza el sidecar completo; no hay compatibilidad parcial.

## 6. Validación del consumidor

Para cada referencia estática del grafo de proyectos, en orden del grafo y sin repetir la misma `.lib`:

1. Sin sidecar: enlace nativo, sin mensaje.
2. Sidecar ilegible o corrupto: respaldo nativo.
3. Identidad de compatibilidad distinta a la del consumidor: respaldo nativo.
4. Digest de biblioteca distinto al SHA-256 de la `.lib` referida: respaldo nativo.
5. Digest de metadata distinto al SHA-256 de la `.nsmeta` referida: respaldo nativo.
6. Número de miembros del archivo COFF distinto al número de unidades: respaldo nativo. Esto garantiza que el
   archivo no tiene objetos (por ejemplo un runtime empaquetado) que el bitcode no reemplaza.

Cada respaldo con sidecar presente se informa como `thinlto-library-fallback: <archivo>: <motivo>` en la salida
del comando. Un respaldo nunca es un error de compilación.

## 7. Consumo

- Las unidades aceptadas se agregan al backend ThinLTO después de las unidades del proyecto, con identificadores
  `nativesharp-thinlto-library-<k>-unit-<j>`. Comparten índice combinado, imports, caché de backends y resoluciones
  de símbolos: la primera definición prevalece y ninguna se internaliza.
- Los objetos de esas unidades reciben rangos de procedimientos CodeView y verificación COFF como cualquier objeto
  ThinLTO.
- Los objetos de cada biblioteca se empaquetan en un archivo COFF que reemplaza a la `.lib` original en la misma
  posición de la lista de enlace. Conservar un archivo, y no pasar los objetos sueltos, mantiene la semántica de
  extracción bajo demanda: un miembro que nada referencia no entra al binario.
- El archivo de reemplazo vive en `%TEMP%\nativesharp-thinlto-libraries\<digest>\<nombre original>.lib`, con
  `<digest>` = 16 hexadecimales del SHA-256 de sus objetos. NSLink escribe la ruta de cada archivo en la tabla de
  módulos del PDB y en el hash de su GUID; una ruta direccionada por contenido mantiene reproducibles los builds
  `-g`. El archivo se escribe como `.partial` y se renombra; entradas sin uso por más de una semana se podan.
- La salida informa `thinlto-libraries: <n>` con el número de bibliotecas consumidas.

## 8. Riesgos y decisiones

- Las funciones de una biblioteca que su NSMETA ya transporta como cuerpo inline siguen llegando como `weak_odr`
  al consumidor. Su copia en el consumidor prevalece por ser la primera definición; LLVM descarta la de la unidad de
  biblioteca y el enlace COMDAT resuelve cualquier duplicado.
- Un símbolo local de la biblioteca referido por una función importada se promueve con el sufijo de hash del
  módulo; el miembro correspondiente del archivo de reemplazo lo define y se extrae por esa referencia.
- El sidecar no participa en la identidad del bundle del consumidor: está ligado a la `.lib`, cuyo digest sí
  participa, y su presencia sólo cambia la optimización, no la semántica.
- El sidecar no se firma más allá del SHA-256 de integridad: protege contra corrupción y desajuste, no contra un
  productor malicioso, igual que la `.lib`.
