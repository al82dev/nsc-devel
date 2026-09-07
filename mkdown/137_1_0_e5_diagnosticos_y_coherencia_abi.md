# 137 — 1.0-E5: diagnósticos y coherencia del contrato ABI

**Fecha:** 2026-09-07  
**Estado:** instalado y certificado por G00  
**Formato:** NSMETA 1.8, sin cambio de bytes  
**Contrato:** Native ABI 1.0 / Runtime ABI 1.0

## Resultado

E5 convirtió la compatibilidad ABI en un diagnóstico explicable. Antes, ocho decisiones físicas se
comparaban juntas y cualquier diferencia producía el mismo mensaje: el contrato era incompatible.
El compilador detenía correctamente el consumo, pero no decía qué debía corregir el productor.

Ahora cada incompatibilidad nombra:

- el componente exacto;
- el valor que acepta el consumidor;
- el valor encontrado en la biblioteca.

Ejemplos de componentes independientes:

- Native ABI y Runtime ABI con par `major.minor`;
- mangling ABI;
- object ABI;
- delegate ABI;
- tamaño físico de la celda delegate;
- exception ABI;
- target id, ancho/alineación de puntero y triple de destino.

Un productor con celda delegate antigua recibe, por ejemplo, una causa del tipo «se esperaban 32
bytes y se encontraron 16». Ya no hace falta inspeccionar el binario ni adivinar cuál de los campos
provocó el rechazo.

## Por qué no cambió NSMETA 1.8

E5 no añadió ni reordenó bytes. La información necesaria ya había sido publicada por E4; el defecto
estaba en cómo se comparaba y explicaba. Subir el formato por mejorar un diagnóstico habría mezclado
la evolución del contenedor con la evolución de su lector.

La función `verifyFrozenAbiContract` se convirtió en la tabla única del contrato físico. La usan:

- `writeModel`, para impedir que el escritor produzca un modelo 1.8 físicamente incompatible;
- `verifyCurrentCompiler`, para detener a un consumidor antes de compilar o enlazar.

Así se evita que escritor y lector mantengan listas distintas de campos ABI.

## Integridad de bytes frente a coherencia de modelo

El checksum y la huella prueban que el archivo no cambió desde que se escribió. No prueban que su
autor haya construido una tabla coherente. Un `.nsmeta` puede tener checksum correcto y aun contener
dos identidades canónicas iguales o una tabla virtual con tres firmas y dos callees.

E5 añade una segunda capa, posterior a la lectura criptográfica y anterior a la materialización:

- identidad de módulo obligatoria en NSMETA 1.8;
- identidad y nombre canónico no vacíos para cada tipo;
- rechazo de identidades de tipo duplicadas;
- una sola tabla de dispatch por tipo;
- igual número de firmas y callees en cada vtable;
- igual número de firmas, implementaciones y callees en dispatch de interfaz;
- claves únicas y no vacías para templates y especializaciones genéricas;
- nombres únicos y no vacíos para exports de la frontera C.

Estas reglas evitan que una incoherencia avance hasta generar fuente sintética, lookup ambiguo o un
fallo tardío del enlazador.

## Compatibilidad histórica preservada

La primera corrida focal detectó cuatro canarios históricos en rojo. Los modelos mínimos NSMETA
1.0/1.1 no llevaban identidad de módulo porque medían únicamente la política de minor anterior; una
fixture 1.8 de templates tampoco la había establecido porque antes no formaba parte de la
verificación estructural.

La corrección fue deliberada:

- los formatos anteriores mantienen su política de lectura;
- la identidad estructural obligatoria comienza en 1.8;
- la fixture del formato actual declara un módulo real antes de probar una plantilla incompleta.

No se rebajó la regla nueva ni se reescribió la historia: se aplicó desde la revisión que puede
prometerla.

## CLI más didáctica

`--verify-library-metadata` ahora imprime, además de la huella:

```text
format: NSMETA 1.8
native-abi: 1.0
runtime-abi: 1.0
mangling-abi: 1
object-abi: 1
delegate-abi: 1
delegate-cell-size: 32
exception-abi: 1
```

Para un archivo anterior imprime que el contrato es implícito y conserva el número de revisión. No
inventa números 1.0 dentro de bytes que nunca los publicaron.

## Archivos y funciones modificados

### `src/Interop/LibraryMetadata.cpp`

- `versionPair`: representación única de versiones ABI `major.minor`.
- `verifyFrozenAbiContract`: comparación y diagnóstico por componente.
- `LibraryMetadataCodec::writeModel`: reutiliza la tabla física común.
- `LibraryMetadataCodec::verifyCurrentCompiler`: valida target, contrato y coherencia estructural.

### `src/Driver/Driver.cpp`

- `printMetadataAbiContract`: muestra el contrato completo sin duplicar su formato en dos órdenes.
- `verifyLibraryMetadata`: incluye formato y componentes ABI en la salida válida.
- `appendValidatedLibraryReferences`: cuando una DLL no coincide, enumera símbolos faltantes y
  sobrantes en lugar de emitir sólo un rechazo genérico.

### Pruebas

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: casos negativos individuales para Native ABI,
  Runtime ABI, mangling, objetos, delegates, tamaño de celda, excepciones, triple, módulo,
  identidades duplicadas, exports duplicados y vtable incoherente.
- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`: separación explícita entre canarios
  mínimos históricos y el contrato estructural 1.8.

## Evidencia

- suite focal de metadata y compatibilidad histórica: 2/2;
- hosts y bibliotecas sensibles: verdes;
- matriz completa del árbol: **297/297**, cero fallos;
- build con warnings tratados como errores: correcto.

La revalidación focal posterior a la limpieza documental y de formato usó:

```text
nsc 0.9.0+build.20260907T182919Z.ceeacff631a7c23c
SHA-256 F3E13714F564D35EE494E71FB8CAE5732EC6459902481E492C4E0E0C7723E748
```

La identidad anterior conserva la evidencia del cierre técnico focal. La reconstrucción instalada
que promovió E5 a producto certificado es:

```text
nsc 0.9.0+build.20260907T191524Z.400a252ae1c4b36a
SHA-256 58C44DD8A966D526D4F83FA9E97BDCD223E3E992AF0D61CCE84E2018D3BA7D5C
```

El G00 `20260907-131523-7eda857d` terminó `LANDED`, con 0 fallos, 11 etapas y 324 casos. E5 queda
cerrado, instalado y certificado junto con E6. El documento 139 conserva el acta completa y explica
por qué la identidad focal y la instalada son distintas sin representar una divergencia.
