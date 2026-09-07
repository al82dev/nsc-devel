# 138 — 1.0-E6: verificación física conjunta de `.lib` y DLL

**Fecha:** 2026-09-07  
**Estado:** instalado y certificado por G00  
**Nueva orden:** `--verify-library-artifacts`

## Resultado

E6 añade una operación pública para responder una pregunta que hasta ahora sólo contestaba el
consumidor durante una compilación completa:

> ¿Estos archivos pertenecen realmente al mismo contrato NativeSharp?

Para una biblioteca estática:

```text
nsc --verify-library-artifacts producto.nsmeta producto.lib
```

Para una DLL con su archivo de importación:

```text
nsc --verify-library-artifacts producto.nsmeta producto.lib producto.dll
```

La orden no confía en nombres de archivo iguales. Abre y valida cada formato, deriva la superficie
física desde NSMETA y la compara con los símbolos reales.

## Qué valida

### 1. Metadata

- magic, longitud y checksum;
- huella ABI recalculada;
- versión del formato;
- target y versión del productor;
- contrato ABI y coherencia estructural de E5.

### 2. Archivo COFF

- formato de archive válido;
- tabla de miembros legible;
- presencia de todos los símbolos requeridos por NSMETA.

La lista requerida une y deduplica:

- exports con nombre estable;
- métodos NativeSharp públicos materializables;
- constructores públicos consumibles;
- accessors de descriptores necesarios para identidad y dispatch entre módulos.

El archivo puede contener símbolos auxiliares. E6 exige el conjunto publicado, pero no rechaza
helpers internos adicionales dentro del archive.

### 3. DLL opcional

- imagen PE válida y marcada como DLL;
- tabla de exports legible;
- igualdad exacta entre exports publicados y exports físicos.

Aquí sí se comparan ambos sentidos. Un símbolo requerido ausente rompe al consumidor; uno inesperado
indica que el producto físico publica una superficie que NSMETA no describe. El diagnóstico enumera
ambas listas por separado.

## Salida reproducible

Una verificación correcta informa:

- módulo;
- rutas de metadata, archive y DLL;
- cantidad de miembros y símbolos del archive;
- cantidad de símbolos requeridos;
- cantidad de exports de la DLL;
- huella ABI;
- todas las versiones físicas del contrato.

Esto permite conservar la salida junto al producto y saber qué se verificó sin depender del perfil
del usuario ni de un proyecto consumidor preparado especialmente.

## Casos negativos añadidos

La prueba no se limita a validar productos correctos. Construye dos productores distintos y cruza
sus piezas de manera controlada:

1. metadata de la DLL con el archivo estático del otro productor;
2. metadata y archivo estático correctos con la DLL del productor contrario.

El primer caso debe fallar nombrando los símbolos requeridos que faltan en el archive. El segundo
debe mostrar a la vez exports faltantes y exports inesperados. Ambos productos son individualmente
válidos; el rechazo demuestra que E6 comprueba pertenencia contractual, no sólo extensiones o magic.

También existen canarios previos que continúan cubriendo:

- archive inválido;
- DLL inválida;
- símbolo publicado ausente;
- runtime con nombre colisionado;
- composición transitiva;
- consumo separado en O0 y O2;
- paridad de lld-link y NSLink en el gate externo.

## Archivos y funciones modificados

### `include/nativesharp/Driver/Driver.h`

- declara `verifyLibraryArtifacts` como operación del driver.

### `src/Driver/Driver.cpp`

- `metadataRequiredSymbols`: construye el conjunto físico publicado.
- `missingSymbols`: diferencia determinista entre dos superficies.
- `describeSymbols`: diagnóstico acotado y legible para listas grandes.
- `printMetadataAbiContract`: salida compartida con el verificador de metadata.
- `Driver::verifyLibraryArtifacts`: coordina NSMETA, COFF y PE.
- `Driver::run`: reconoce la variante estática y la variante con DLL.
- `Driver::printHelp`: documenta ambas formas mediante el argumento opcional `[file.dll]`.

### `tests/unit/Stage48_2LibraryMetadataTests.cpp`

- verifica un producto DLL completo;
- verifica una biblioteca NativeSharp estática sin exports de host;
- cruza metadata y archive de productores distintos;
- cruza metadata/archive correctos con una DLL ajena;
- exige que la salida incluya huella, clase de producto y contrato ABI.

## Lo que esta orden no afirma

La verificación física no ejecuta las funciones y no sustituye las fixtures O0/O2. Prueba que los
tres archivos describen y contienen la misma superficie. La semántica de las funciones continúa
perteneciendo al corpus de ejecución.

La separación es útil: una biblioteca puede tener símbolos correctos y una implementación errónea,
o una implementación correcta empaquetada con metadata de otro build. Son fallos distintos y ahora
tienen gates distintos.

## Evidencia

- producto estático correcto: aceptado;
- DLL + import archive correctos: aceptados;
- archive de otro productor: rechazado con símbolos faltantes;
- DLL de otro productor: rechazada con faltantes y sobrantes;
- Stage 46 y Stage 48.2: verdes;
- matriz completa del árbol: **297/297**, cero fallos.

Las tres suites contractuales Stage 46, Stage 48.2 y Stage 50.9 pasaron también en la construcción
alternativa. Después de la revisión de formato, Stage 48.2 y Stage 50.9 se recompilaron y volvieron a
pasar con el binario focal:

```text
nsc 0.9.0+build.20260907T182919Z.ceeacff631a7c23c
SHA-256 F3E13714F564D35EE494E71FB8CAE5732EC6459902481E492C4E0E0C7723E748
```

La identidad anterior conserva la evidencia del cierre técnico focal. La reconstrucción instalada
que promovió E6 a producto certificado es:

```text
nsc 0.9.0+build.20260907T191524Z.400a252ae1c4b36a
SHA-256 58C44DD8A966D526D4F83FA9E97BDCD223E3E992AF0D61CCE84E2018D3BA7D5C
```

El G00 `20260907-131523-7eda857d` terminó `LANDED`, con 0 fallos, 11 etapas y 324 casos. E6 queda
cerrado, instalado y certificado junto con E5. El acta completa y la relación entre la evidencia
focal y el producto instalado están en el documento 139.
