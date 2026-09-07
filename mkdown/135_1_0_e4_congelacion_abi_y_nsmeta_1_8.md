# 135 — 1.0-E4: congelación ABI y NSMETA 1.8

## Resultado de la entrega

E4 separa por primera vez tres identidades que antes estaban mezcladas:

1. versión del compilador que produjo el artefacto;
2. versión del formato de archivo NSMETA;
3. versión del contrato binario que productor y consumidor deben compartir.

El formato actual es **NSMETA 1.8**. El contrato descrito es **Native ABI 1.0** y **Runtime ABI
1.0**. Un futuro NSMETA 1.9 puede agregar información sin fingir que cambió el layout de objetos o
la convención de llamada. Y si cambia una decisión física, el compilador puede rechazarla antes de
generar LLVM o invocar al linker.

## Campos congelados

NSMETA 1.8 añade al payload raíz:

| Campo | Valor 1.0 | Qué protege |
|---|---:|---|
| `nativeAbiMajor/minor` | 1.0 | contrato binario general |
| `runtimeAbiMajor/minor` | 1.0 | funciones y estado del runtime |
| `manglingAbiVersion` | 1 | nombres de símbolos NativeSharp |
| `objectAbiVersion` | 1 | referencias, descriptors, vtables e interfaces |
| `delegateAbiVersion` | 1 | celda code/context/destroy/free |
| `exceptionAbiVersion` | 1 | SEH, payload, RTTI y cleanup |
| `managedDelegateCellSize` | 32 | tamaño físico verificable de la celda |

Los anchos son parte de la codificación. No son texto informativo ni se calculan a partir de la
versión del ejecutable.

## Qué cubre la huella ABI

`encodeAbiProjection` deriva del payload completo y sólo borra `producerVersion`, porque el build id
identifica una compilación pero no altera por sí mismo el contrato. Por tanto la SHA-256 cubre:

- módulo y target;
- versión ABI y de cada familia física;
- tipos, tamaños, alineaciones, offsets y tipos subyacentes;
- firmas, qualifiers de ownership, `ref/out/in` y defaults;
- nombres mangled y símbolos C publicados;
- jerarquía, type ids, descriptors, vtables, interfaces y destruction entries;
- templates públicos y especializaciones genéricas;
- representación de delegates y versión de excepciones.

Dos compilaciones del mismo contrato, aunque tengan build ids distintos, producen la misma huella.
Cambiar una decisión pública produce otra huella.

## Golden ABI 1.0

La suite conserva un modelo raíz mínimo con todos los contadores ABI y ningún tipo. Su fingerprint
revisado es:

```text
30b4f83c4ba0db0a507b512828508df6802c973f3cbbd60dde59080b45f8c8b3
```

No pretende reemplazar las fixtures ricas. Su propósito es detectar el cambio más fácil de esconder:
reordenar o cambiar el ancho de un campo raíz y actualizar lector/escritor a la vez. En ese caso el
round trip seguiría verde, pero el golden fallaría.

Las fixtures ricas siguen comprobando sensibilidad a firmas, defaults, signo de enums, layouts,
jerarquía y símbolos.

## Compatibilidad hacia atrás

El escritor puede producir modelos anteriores y el lector actual los consume con ramas por minor.
La nueva prueba genera físicamente un NSMETA 1.7:

- no escribe los campos ABI 1.8;
- el lector no inventa valores: los expone como cero/no declarado;
- templates y especializaciones posteriores permanecen alineados;
- `verifyCurrentCompiler` lo acepta bajo la política legacy existente.

La compatibilidad no se extiende artificialmente a información que 1.7 nunca tuvo. Por ejemplo, un
delegate ordinario importado necesita el contrato 1.8 para demostrar la forma de su celda; el
callback crudo que ya estaba descrito desde 1.5 continúa materializable.

## Rechazo temprano

`writeModel` y `verifyCurrentCompiler` comparan el contrato 1.8 con las constantes que implementa el
compilador. Se rechazan antes de compilar al consumidor:

- major ABI diferente;
- minor ABI mayor al soportado;
- runtime incompatible;
- otra versión de mangling, objeto, delegate o excepción;
- otro tamaño de celda managed delegate;
- target, tamaño o alineación de puntero diferentes;
- carrier de referencia incompatible en clases, interfaces o delegates.

La prueba modifica por separado Delegate ABI y Exception ABI y exige el diagnóstico del contrato,
no un fallo tardío por símbolo ausente.

## Símbolos públicos e internos

El contrato no convierte todo símbolo en API. La selección permanece en dos niveles:

- `isPublicNativeMethod` y `canMaterializePrecompiledDeclaration` deciden qué declaraciones puede
  reconstruir un consumidor;
- `publishedNativeSymbols` calcula los símbolos que el archive o import library debe contener.

Callables sintéticos de closures, helpers privados y detalles del runtime pueden ser direcciones
guardadas dentro de una celda sin convertirse en nombres públicos. El consumidor llama el puntero
entregado por el productor; no intenta resolver el nombre interno.

## Relación con la ABI física del target

Esta entrega congela decisiones ya medidas, no inventa otra convención:

- punteros y referencias: 8 bytes, alineación 8;
- structs directos/indirectos: las reglas Win64 ya comparadas con clang-cl/MSVC;
- retornos agregados: `sret` en las posiciones certificadas;
- C ABI: wrappers, callbacks y host fixtures de Stage 47;
- clases: descriptor, vtable, tablas de interfaz y destruction entry de E1;
- delegates: celda de E3;
- excepciones: `RaiseException`, `__C_specific_handler`, type ids estables y payload Win64.

Las pruebas de Stage 46 agregan ahora la comprobación que faltaba a la comparación de forma:
productor y consumidor compilados por separado ejecutan el mismo contrato en `.lib` y DLL.

## Política de cambios incompatibles

1. Un campo aditivo de metadata sube `formatMinor`; no mueve la versión ABI si no cambia bytes de
   programa, símbolos ni semántica entre módulos.
2. Una ampliación compatible del contrato sube el minor ABI y debe conservar lectores del minor
   anterior.
3. Un cambio de layout, calling convention, mangling o significado existente sube el major de la
   familia y, cuando afecte al conjunto, `nativeAbiMajor`.
4. Nunca se reutiliza un número para otra representación.
5. Todo cambio ABI exige fixture negativa de rechazo temprano, golden actualizado de manera
   explícita y capítulo de migración.
6. Un bug que pueda corromper memoria no se conserva sólo para mantener una huella; se corrige y se
   versiona el contrato.

## Archivos y funciones modificados

### Modelo público

- `include/nativesharp/Interop/LibraryMetadata.h`
  - `kMetadataFormatMinor = 8`;
  - constantes Native/Runtime/Mangling/Object/Delegate/Exception ABI;
  - `kManagedDelegateCellSize = 32`;
  - campos raíz correspondientes en `LibraryMetadata`.

### Codec y verificador

- `src/Interop/LibraryMetadata.cpp`
  - `buildMetadataModel`: congela carrier de referencia para delegates;
  - `encodePayload`: escribe el contrato sólo desde minor 8;
  - `decodePayload`: lo lee con validación de truncamiento;
  - `LibraryMetadataCodec::read`: marca como no declarado el contrato de 1.0–1.7;
  - `writeModel`: impide producir una combinación que este compilador no implementa;
  - `verifyCurrentCompiler`: rechaza consumidores incompatibles antes del backend;
  - `encodeAbiProjection`: incluye automáticamente los nuevos campos en la huella.

### Consumidor de bibliotecas

- `src/Driver/Driver.cpp`
  - exige Delegate ABI 1 y tamaño 32 para materializar delegates ordinarios.

### Pruebas

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - round trip 1.8;
  - campos del contrato;
  - delegate ordinario frente a callback crudo;
  - rechazo de versiones físicas incompatibles;
  - golden SHA-256.
- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`
  - producción y lectura real de NSMETA 1.7;
  - ausencia explícita de campos 1.8;
  - alineación de templates/especializaciones;
  - aceptación legacy documentada.
- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - misma identidad y comportamiento al cruzar `.lib` y DLL en O0/O2.

## Gate de salida de E4

E4 se considera cerrado porque:

- mismo modelo público produce mismos bytes y misma huella;
- un cambio público relevante altera la huella;
- un contrato físico incompatible se rechaza antes del link;
- un archivo 1.7 se lee sin desplazar ni inventar campos;
- el orden raíz tiene golden estable;
- los símbolos requeridos se verifican contra el producto físico;
- delegates, objetos y excepciones usan las versiones que NSMETA declara.

Lo siguiente ya no es diseñar otra ABI. Es estabilizar la ABI 1.0 existente con más corpus,
diagnósticos y reproducción limpia hasta la release candidate.

## Correcciones encontradas por la matriz completa

La primera corrida integral fue útil porque encontró cinco aserciones que seguían certificando el
contrato anterior:

- Stage 42 esperaba una celda delegate de 16 bytes;
- Stage 47 esperaba sólo métodos y omitía los descriptores públicos de tipos requeridos por RTTI;
- los hosts Stage 48.5 y 48.6 seguían contando 42 exports, aunque la superficie física completa es
  ahora 45: 21 wrappers estables, 21 métodos NativeSharp, un constructor y dos class descriptors;
- Stage 49 esperaba el diagnóstico de `raii` anterior a delegates y buffers ownables.

Las pruebas se actualizaron contra la superficie física observada con `PeExecutableVerifier`; no se
cambiaron para aceptar “cualquier cantidad”. Stage 47 fija 24 exports en su fixture, Stage 48 fija
45 y Stage 42 busca los offsets de destructor de contexto y liberador de celda.

La prueba del host administrado también dependía accidentalmente del perfil roaming del usuario.
`Stage48_5CSharpPInvokeHostTests.cpp` crea ahora un perfil aislado y un `NuGet.Config` sin feeds: el
ejemplo no consume paquetes, por lo que esta configuración elimina credenciales y estado externo sin
reducir la compilación real del host.

Finalmente se endureció `LlvmTargetMachine::emit`. Windows puede conservar unos milisegundos un COFF
recién verificado como sección mapeada; al sobrescribir la fixture, LLVM devolvía
`ERROR_USER_MAPPED_FILE`. El emisor reintenta únicamente esa familia exacta de errores de sharing,
con pausas de 10 ms y un límite total de un segundo. Errores de IR, destino o filesystem distintos
siguen fallando en el primer intento. Esto hace reproducible la emisión física sin convertir fallos
reales en éxitos.

Archivos adicionales ajustados durante este cierre:

- `src/LLVM/LlvmTargetMachine.cpp`: reintento acotado de output COFF transitoriamente mapeado;
- `tests/unit/Stage42DelegatesEventsTests.cpp`: contrato de celda de cuatro punteros;
- `tests/unit/Stage47HostCAbiTests.cpp`: exports de type descriptors y RTTI;
- `tests/unit/Stage48_5CSharpPInvokeHostTests.cpp`: conteo físico y perfil SDK aislado;
- `tests/unit/Stage48_6CppCliHostClosureTests.cpp`: conteo físico completo;
- `tests/unit/Stage49RaiiMoveSemanticsTests.cpp`: diagnóstico ownable vigente.

## Evidencia local del 7 de septiembre de 2026

- matriz dirigida E3/E4: **5/5**;
- emisiones `.obj` históricas con el binario nuevo: **11/11**;
- prueba de bloqueo controlado: el proceso permaneció activo mientras el COFF estuvo mapeado y
  terminó con exit 0 al liberarse 300 ms después;
- catálogo completo apuntando a `build-tests/bin/nsc.exe`: **297/297**, cero fallos;
- Stage 46 está verde en las dos construcciones y ejecuta `.lib`/DLL, O0/O2;
- NSMETA 1.8, golden fingerprint y lector real 1.7 están verdes en las dos construcciones.

El binario de esta corrida se identifica como:

```text
nsc 0.9.0+build.20260907T170755Z.d17a6c16e692945b
SHA-256 AE811A9E1B0A59FD08F4982938A2530BCE63954FDBA9A4D20F000D777DD8D52E
```

El número de release permanece en 0.9.0 mientras el trabajo hacia 1.0 no haya sido instalado y
certificado. La ABI interna ya declara 1.0 porque ese número identifica el contrato binario, no la
etiqueta comercial del ejecutable; E4 separó precisamente ambas identidades.

La carpeta `build-test-nslink` conserva deliberadamente
`NSC_TEST_COMPILER=C:/native-sharp/bin/nsc.exe`. Por eso sus pruebas CLI todavía miden el binario
instalado anterior hasta que se haga la instalación; los ejecutables unitarios de esa carpeta sí
fueron recompilados contra el árbol actual. No se sustituyó silenciosamente el instalado durante
esta entrega. El cierre externo correcto es instalar el nuevo hash y ejecutar G00 con el árbol
inmóvil.
