# 129 — Cierre de NativeSharp 0.9.0

**Fecha:** 2026-09-06  
**Release:** `nsc 0.9.0`  
**Dominio:** compilador nativo, `.lib`, DLL, metadata y consumo entre módulos.  
**Fuera de propósito:** GUI, web, SQL, LINQ y aplicaciones empresariales.

## Resultado

NativeSharp 0.9.0 deja de tratar una biblioteca como un conjunto de objetos que sólo el linker
entiende. Una biblioteca NativeSharp es ahora un producto reutilizable formado por:

1. código nativo en `.lib` o DLL más import library;
2. identidad de módulo y símbolos NativeSharp;
3. contrato fuente y ABI en `.nsmeta`;
4. validación física antes del enlace;
5. composición transitiva sin copiar las fuentes del productor.

El consumidor usa nombres NativeSharp normales. No necesita escribir un espejo `extern C`, no
vuelve a generar el cuerpo de la biblioteca y no inventa layouts o ownership que el productor no
haya publicado.

## Qué quedó cerrado

### Productos físicos

- `.lib` COFF estática con índice de símbolos verificado;
- DLL PE32+ x64 con import `.lib` correspondiente;
- selección independiente del linker PE y del archivador estático;
- paridad funcional entre lld-link y NSLink;
- composición A → B → ejecutable, incluida DLL B que incorpora A estática;
- el ejecutable final importa únicamente las DLL que realmente sobreviven en el grafo.

### Superficie NativeSharp precompilada

- métodos públicos estáticos, overloads y argumentos predeterminados;
- parámetros `Value`, `ref`, `out` e `in`;
- ownership `owned`, `borrowed` y transferencia mediante `move`;
- structs naturales, empacados, alineados y anidados;
- offsets, tamaño final y alineación comprobados en el consumidor;
- enums con ancho, signo, tipo subyacente y constantes exactos;
- métodos concretos de instancia sobre structs;
- clases opacas como referencias nativas;
- métodos concretos no virtuales sobre receptores de clase opaca;
- factories que devuelven ownership y sinks que lo consumen y destruyen;
- callbacks `extern C delegate` reconstruidos desde metadata.

### Validación física

El consumidor no confía sólo en que el texto del tipo coincida. Antes de enlazar comprueba:

- target y ABI de puntero;
- formato de metadata y versión de productor;
- fingerprint ABI y checksum del payload;
- tamaño, alineación y offsets de tipos de valor;
- tamaño/alineación del carrier de referencia de clases e interfaces;
- existencia de los símbolos NativeSharp y C publicados;
- coincidencia entre DLL, import library y metadata.

Una metadata autofirmada pero falsa sigue siendo rechazada. Las pruebas alteran deliberadamente
un `Packet` de 13 a 12 bytes y una referencia de clase de 8 a 4 bytes; ambos casos se detienen antes
de producir el ejecutable.

## NSMETA 1.5

NSMETA 1.5 agrega a cada tipo delegate:

- indicador de representación `extern C`;
- tipo de retorno canónico;
- lista ordenada de tipos de parámetros.

La firma participa en el fingerprint ABI. Un consumidor ya no sabe únicamente que existe un tipo
llamado `Transform`: puede reconstruir exactamente `int Transform(int)` y usarlo como callback de
una `.lib` o DLL.

El lector mantiene compatibilidad con 1.0–1.4. La prueba del predecesor 1.4 demuestra que la
ausencia de los nuevos campos no consume accidentalmente el contador de campos que sigue en el
registro ni desplaza los miembros posteriores.

## Clases opacas y ownership

Una clase precompilada se materializa como referencia opaca. El consumidor no conoce ni toca sus
campos privados y no asigna el objeto por tamaño. La biblioteca crea el objeto y publica la
referencia:

```csharp
raii Precompiled.Counter value = Precompiled.Objects.Create(40);
int answer = value.Add(2);
Precompiled.Objects.Destroy(move(value));
```

Este patrón conserva cuatro propiedades:

1. la asignación física ocurre en el productor;
2. el receptor de `Add` usa el símbolo mangled del productor;
3. un préstamo no crea un segundo dueño;
4. la transferencia a `Destroy` invalida el local y el cleanup del parámetro libera una sola vez.

La primera prueba encontró un doble free porque el fixture hacía `delete value` dentro de un
parámetro `owned`: el cleanup automático del parámetro ya era el dueño de esa destrucción. El
fixture corregido deja el cuerpo vacío y comprueba la regla real del lenguaje, no un workaround.

Los métodos virtuales y de interfaz no se degradan a llamadas directas. Se omiten hasta que el
consumidor pueda hidratar las tablas de dispatch del productor. Esa protección evita que 0.9 gane
aparente compatibilidad rompiendo polimorfismo.

## Callbacks precompilados

El productor puede declarar:

```csharp
public extern C delegate int Transform(int value);
public static int Invoke(Transform callback, int value);
```

El consumidor reconstruye `Transform`, convierte un método estático compatible en callback y lo
entrega a `Invoke`. La llamada completa ejecuta 42 desde `.lib` y DLL, en O0 y O2, con lld-link y
NSLink.

## Evidencia

La suite de bibliotecas Stage 46 creció de 74 a **111 comprobaciones**. Incluye productores y
consumidores separados, ejecución real y negativos previos al enlace.

- Stage 46 con lld-link: **111/111**.
- Stage 46 con NSLink: **111/111**.
- Metadata Stage 48.2: **72/72**.
- Compatibilidad de metadata Stage 50.9: **47/47**.
- Matriz directa de cierre: **5/5** con cada linker.
- Catálogo nativo completo: **296/296 efectivos** con cada linker.
- Host C# P/Invoke: **1/1** con cada linker.
- Total funcional: **297/297** con lld-link y **297/297** con NSLink.

La revalidación final del producto estable recorrió los 297 casos en serie. Obtuvo 288 verdes
directos; ocho emisiones de `.obj` que Windows rechazó temporalmente por una sección mapeada
abierta pasaron 8/8 al repetirse aisladas, y el host C# P/Invoke pasó 1/1 con acceso al SDK. La
composición final es 288 + 8 + 1 = **297/297 efectivos**. No se cambió código para ocultar el
comportamiento del sistema de archivos ni se contó un fallo de compilador como verde.

## Identidad del producto

- versión: `nsc 0.9.0+build.20260907T035128Z.7cc7faa5ecc8a96b`;
- lenguaje: `NativeSharp 0.1`, Stage 57;
- SHA-256: `C3C2F2067860647EF8513330B542F97C53F3D4B3A4CE017D3BA88902BE2CD5FD`;
- tamaño: `3,713,024` bytes;
- generado: `2026-09-07T03:51:28Z` UTC;
- producto: `build-tests/bin/nsc.exe`.

## Qué no promete 0.9.0

0.9.0 cierra la frontera binaria reutilizable, no toda la meta de 1.0. Quedan para la siguiente
línea:

- construir directamente desde el consumidor una clase cuyo layout vive en otra biblioteca;
- dispatch virtual y de interfaces hidratado desde metadata;
- instanciar en el consumidor una especialización genérica nueva que no exista en el productor;
- delegates NativeSharp con closure/target, distintos del callback crudo `extern C`;
- congelación pública definitiva de mangling/runtime ABI para 1.0.

Estas exclusiones pertenecen al compilador y están descritas con precisión. No abren trabajo de
web, SQL, LINQ, GUI o enterprise: esos dominios no son y no serán parte del propósito del lenguaje.

## Estado de entrega

El árbol local y el ejecutable están cerrados como `0.9.0`. La instalación y G00 deben ejecutarse
sobre el hash exacto anterior para convertir este cierre técnico en baseline instalado certificado.
