# 123 — Plan final de implementación de NativeSharp 0.8.0-alpha

**Fecha:** 2026-09-05  
**Producto:** compilador y toolchain NativeSharp para Windows x64  
**Entrega:** una sola entrega pública al completar todos los gates  
**Versión objetivo:** `0.8.0-alpha`  
**Revisión ABI inicial:** `NSABI 1`

---

## 1. Objetivo

NativeSharp 0.8.0-alpha debe ser una base nativa verificable para software Windows de tiempo real:

- videojuegos;
- motores gráficos;
- render y cómputo gráfico;
- audio;
- herramientas de engine;
- integración con Win32, COM, DXGI, Direct3D, DirectXMath y otras APIs nativas;
- job systems, arenas, pools y código de frame crítico.

El objetivo no es convertir NativeSharp en un lenguaje general para web, servidores, SQL o
aplicaciones empresariales. El diseño y las prioridades se deciden por ABI de Windows, rendimiento,
layout, memoria, depuración y tiempos de iteración.

La dirección de trabajo es:

> representar exactamente → verificar con autoridades independientes → poder depurar → construir
> COM → añadir SIMD y control de memoria → congelar y certificar la ABI.

---

## 2. Qué significa «una sola entrega»

Se separan dos conceptos:

- **Aterrizar:** un gate terminó verde y su código verificado queda guardado en `A:\nsc-devel`.
  Ocurre después de cada gate.
- **Entregar:** se publica la certificación completa, el reporte final y la propuesta de cambiar la
  versión a `0.8.0-alpha`. Ocurre una sola vez, después del Gate 12.

El trabajo se aterriza por gates porque una sesión o contenedor de trabajo no es almacenamiento
durable. «Una sola entrega» no significa mantener cambios verificados fuera del disco hasta el
final.

La versión pública no se modifica durante el desarrollo. El cambio a `0.8.0-alpha` sólo se propone
después de cerrar el Gate 12.

---

## 3. Reglas de trabajo

1. La autoridad es el contenido actual del disco, nunca el último commit.
2. Cada fichero se vuelve a leer inmediatamente antes de editarlo.
3. No se hacen commits salvo autorización posterior explícita.
4. No se ejecuta `all.bat` ni se crean nuevos BAT/CMD.
5. La construcción, pruebas, certificación, archivado y captura de logs se orquestan desde
   `nscbuilder.py`.
6. No se añaden scripts Python auxiliares para repartir responsabilidades de `nscbuilder.py`.
7. Los logs son archivos normales; no se usa SQLite ni otra base de datos.
8. El compilador se construye siempre con clang/LLVM y lld-link.
9. El linker seleccionable sólo aplica a los programas de prueba y productos NativeSharp.
10. Los árboles de compilación permanecen separados:

    ```text
    build-compiler/
    build-test-lldlink/
    build-test-nslink/
    ```

11. Limpiar pruebas nunca puede borrar el compilador.
12. Cada gate ejecuta primero pruebas dirigidas y después su matriz contractual.
13. Un gate no se marca `LANDED` mientras conserve fallos, pendientes obligatorios o casos sin
    ejecutar.
14. La entrega final no se declara verde a partir de contadores agregados: se verifican los logs,
    manifiestos, casos y artefactos exactos.

---

## 4. Persistencia entre gates

### 4.1 Checkpoint ordinario

Cada gate guarda:

- fuentes necesarias para reconstruir o un snapshot reproducible de ellas;
- manifiesto canónico de rutas y hashes;
- lista de archivos cambiados;
- logs crudos;
- configuración de pruebas;
- resultados por caso;
- estado `IN_PROGRESS`, `FAILED` o `LANDED`.

No copia binarios ni toolchains completos.

```text
A:\nsc-devel\_abi\gates\G00\
A:\nsc-devel\_abi\gates\G01\
...
```

### 4.2 Archivo contractual

Se crea:

- en Gate 0 como registro histórico y red de seguridad;
- después del Gate 2 al declarar `abi-core-v1`;
- la primera vez que cada perfil ABI queda completamente certificado;
- para cada release candidate que deba participar como N−1.

Contiene:

- `nsc.exe`;
- runtime;
- `.obj`, `.lib` y `.dll` necesarios para la matriz;
- headers generados;
- archivos `.nsmeta`;
- fuentes exactas;
- oracle C/C++ y sus artefactos;
- versión de LLVM, Windows SDK y compiladores C/C++;
- defines, flags, generador, arquitectura y linkers;
- hashes SHA-256;
- resultados y certificados;
- referencia al toolchain por digest.

### 4.3 Toolchains deduplicados

Los toolchains no se copian dentro de cada perfil. Se almacenan o referencian una sola vez:

```text
A:\nsc-devel\_abi\toolchains\<digest>\
A:\nsc-devel\_abi\archives\<perfil>\<build-id>\
```

El digest se calcula sobre un manifiesto canónico ordenado de:

```text
(ruta relativa normalizada, tamaño, sha256)
```

Reglas del manifiesto:

- rutas relativas al root del toolchain;
- `/` como separador;
- codificación UTF-8;
- orden byte a byte;
- rechazo de colisiones de rutas que sólo difieran en mayúsculas;
- JSON canónico o registros con longitud prefijada;
- sin timestamps como parte de la identidad.

El hash de un ZIP identifica únicamente ese contenedor de transporte. Nunca sustituye al digest del
contenido.

Si la licencia impide copiar el Windows SDK, se conservan su versión, hashes, referencia de
recuperación, headers preprocesados y oracles ya compilados.

---

## 5. Modelo de conformidad ABI

### 5.1 Estados por caso

- `PASSED`: se ejecutó y cumplió el contrato.
- `FAILED`: se ejecutó y divergió.
- `PENDING`: el contrato está escrito, pero la feature todavía no existe.
- `NOT_RUN`: la feature existe, pero faltó SDK, compilador, arquitectura o capacidad de ejecución.

`PENDING` y `NOT_RUN` nunca cuentan como aprobados.

Cada `PENDING` declara:

- feature que falta;
- gate propietario;
- perfil ABI;
- motivo;
- revisión desde la que pasa a ser obligatorio.

### 5.2 Estados globales

- `CERTIFIED`: cero fallos, cero pendientes y cero casos obligatorios sin ejecutar.
- `CERTIFIED FOR <perfil>`: el perfil nombrado está completo, aunque otros sigan pendientes.
- `INCOMPLETE`: existen contratos `PENDING`.
- `NOT CERTIFIED`: faltó ejecutar algún caso obligatorio.
- `FAILED`: al menos un caso ejecutado divergió.

### 5.3 Huellas independientes

El certificado conserva dos digests:

- `contract-set-digest`: identidad, descripción, firma, dirección y contrato de cada caso;
- `result-set-digest`: estado y resultado de cada caso en esa corrida.

Pasar de `PENDING` a `PASSED` cambia el digest de resultados, pero no el del contrato. Añadir,
eliminar o cambiar un caso modifica el digest contractual aunque el contador total parezca válido.

### 5.4 Autoridades independientes

`nsc` no puede generar el modelo NativeSharp y también toda la verdad esperada.

- `nsc` genera símbolos, bridges, manifiestos y scaffolding.
- El lado C/C++ independiente construye valores, llama, recibe y compara.
- El Windows SDK es la autoridad para tipos, firmas y vtables públicas.
- Clang-cl y MSVC son oracles separados.
- Si Clang-cl y MSVC discrepan, la prueba falla aunque NativeSharp coincida con uno.
- Los valores esperados nunca se derivan del mismo modelo de layout que se está comprobando.

### 5.5 Patrones de datos

- Cada parámetro, campo e invocación utiliza patrones distintos.
- Los enteros usan bytes derivados de una semilla contractual estable.
- Los floats usan valores finitos, normales, distintos y exactamente representables.
- Los variádicos usan, entre otros, `1.5`, `2.25` y `6.75`.
- No se usan ceros, unos ni patrones simétricos como controles principales.
- No se comparan agregados completos con `memcmp`: el padding no tiene valor contractual.
- Los bitfields comparan únicamente bits definidos.
- Los punteros contienen direcciones válidas distinguibles, no bits arbitrarios.
- Canarios externos detectan escrituras fuera del objeto.

---

## 6. Perfiles ABI y gate propietario

| Perfil | Se congela | Primer archivo contractual |
|---|---:|---:|
| `abi-core-v1` | G2 | después de G2 |
| `abi-layout-msvc-v1` | G3a | después de G3a |
| `abi-bitfields-msvc-v1` | G3b | después de G3b |
| `abi-varargs-win64-v1` | G3c | después de G3c |
| `abi-sdk-contract-v1` | G4 | después de G4 |
| `abi-callbacks-v1` | G4 | después de G4 |
| `abi-overaligned-v1` | G4 | después de G4 |
| `abi-com-v1` | G6 | después de G6 |
| `abi-vectorcall-v1` | G8 | después de G8 |

El callback mínimo de G2 demuestra la máquina, pero no congela `abi-callbacks-v1`. Ese perfil sólo
queda completo en G4 después de cubrir ambas direcciones y las fronteras de excepciones.

---

## 7. Frontera arquitectónica de LLVM

El compilador conserva la API C como frontera:

- no se incluyen headers C++ de LLVM como efecto secundario de un criterio de cierre;
- `LlvmCApi.h` sigue aislando el frontend y backend de diferencias innecesarias entre LLVM;
- cualquier puente C++ futuro requiere una decisión arquitectónica explícita y documentada;
- si hiciera falta, sería pequeño, aislado y expondría una API C propia.

Para Gate 1 se usa primero `LLVMPassBuilderOptionsSetDebugLogging`.

En Windows el registro se captura **en el límite del proceso**: `nsc` crea un hijo con `stderr`
redirigido antes de que `LLVM-C.dll` inicialice su CRT. No se usa `_dup2` dentro del proceso,
porque sólo modifica el descriptor de `nsc` y no el de la DLL. El hijo delimita las ventanas de
LLVM y el padre separa el registro de los diagnósticos ordinarios.

El reporte puede decir:

```text
último pass observado: ...
última función/unidad observada: ...
```

No puede llamarlo «pass culpable»: una secuencia de log prueba orden de ejecución, no causalidad.

---

## 8. Gates de implementación

## Gate 0 — preservar y certificar el baseline actual

No modifica el compilador.

### G0.1 Archivo histórico inmediato

Antes de reconstruir:

- copiar el `nsc.exe` instalado actual;
- guardar runtime, bibliotecas, headers y metadata disponibles;
- asociar los últimos logs verdes como evidencia histórica;
- registrar que la asociación todavía no es un baseline certificado si los logs no contienen el
  hash exacto del binario.

### G0.2 Baseline reproducible del disco actual

1. Resolver desde CMake la ruta exacta del compilador construido.
2. Generar el manifiesto inicial de fuentes `S0`.
3. Construir e instalar sin modificar fuentes.
4. Calcular:
   - `B0`: hash del `nsc.exe` construido;
   - `I0`: hash del `nsc.exe` instalado.
5. Exigir `B0 == I0`.
6. Registrar hashes de `lld-link.exe`, `nslink.exe` y ejecutables CTest.
7. Ejecutar CTest, fixtures y bug triage.
8. Calcular de nuevo `S1`, `B1`, `I1` y los hashes de herramientas/pruebas.
9. Exigir:
   - `S0 == S1`;
   - `B0 == B1`;
   - `I0 == I1`;
   - `B1 == I1`;
   - ninguna herramienta ni ejecutable de prueba cambió durante la corrida.
10. Crear el baseline certificado de G0.

El manifiesto fuente incluye `src`, `include`, `lib`, `tests`, `examples`, `CMakeLists.txt`,
`nscbuilder.py` y los runners/tablas usados. Excluye `.git`, builds, logs, `_abi`, `_nscbuilder`,
cachés y temporales.

### Cierre G0

- Archivo histórico preservado.
- Baseline actual ligado a hashes exactos.
- Código fuente, binarios y corpus permanecieron inmóviles durante la medición.
- Estado guardado en `_abi/gates/G00/`.

---

## Gate 1 — cerrar NSC-LIMIT-005

Archivos principales:

- `src/LLVM/LlvmOptimizer.cpp`;
- `src/LLVM/LlvmTargetMachine.cpp`;
- `src/LLVM/ProjectOopLlvmLowerer.cpp`;
- pruebas `c38` y `b9`;
- fixtures diferenciales O0/O2.

Trabajo:

- registrar LLVM, pipeline y nivel de optimización;
- verificar el módulo antes y después de optimizar;
- capturar el último pass y unidad observados;
- detectar errores, falta de convergencia o tiempo excesivo;
- conservar el arreglo de enum estrecho negativo;
- demostrar que la instrumentación no cambia el IR sano;
- añadir pruebas diferenciales O0/O2.

### Cierre G1

La afirmación se limita al corpus identificado:

- 291/291 CTest por configuración aplicable;
- 132/132 fixtures con lld-link;
- 132/132 fixtures con NSLink;
- 18/18 bug triage con lld-link;
- 18/18 bug triage con NSLink;
- misma semántica observable O0/O2 sobre esas fixtures y bug triage;
- un fallo de optimización identifica LLVM, pipeline, función/unidad y último pass observado.

No se afirma «cero divergencias posibles» fuera de ese conjunto.

---

## Gate 2 — máquina ABI mínima demostrada

Archivos principales:

- `src/Driver/Driver.cpp`;
- nuevos componentes bajo `src/Interop` e `include/nativesharp/Interop`;
- `src/Interop/CHeaderGenerator.cpp`;
- `src/Interop/LibraryMetadata.cpp`;
- `nscbuilder.py`;
- nuevas pruebas C/C++ y NativeSharp.

Implementar:

- perfiles y estados;
- `NSABI` independiente de versión del compilador y de `.nsmeta`;
- manifiestos y digests;
- certificados por perfil;
- captura por linker y optimización;
- vista ABI y errores en `nscbuilder.py`;
- oracle C/C++ independiente;
- bridge NativeSharp mínimo.

Casos verticales iniciales:

1. función libre escalar;
2. agregado de 4 bytes;
3. agregado de 8 bytes que contiene floats;
4. agregado de 24 bytes mediante `sret`;
5. receptor equivalente a `this` más `sret`;
6. callback C → NativeSharp;
7. llamada NativeSharp → C;
8. vtable cruda mínima de varios slots.

Mutaciones obligatorias:

- offset incorrecto;
- parámetros intercambiados;
- slot incorrecto;
- retorno truncado;
- caso eliminado o cambiado sin actualizar el digest.

Cada mutación debe volver roja la certificación.

### Cierre G2

- La máquina funciona de extremo a extremo.
- Clang-cl y MSVC coinciden para el perfil core.
- O0/O2 y ambos linkers pasan.
- Las mutaciones se detectan.
- Se declara `NSABI 1` y se congela `abi-core-v1`.
- Se crea inmediatamente su primer archivo contractual, que será su N−1 real.

---

## Gate 3a — layout estructural de Windows/MSVC

Implementar verticalmente en Parser, AST, NameLookup, MemberModel, ObjectLayoutModel,
TargetDataLayout, HIR, MIR y LLVM según corresponda:

- structs anónimos;
- unions anónimos;
- alineación por campo;
- arrays fijos anidados;
- arrays finales declarados `[1]`;
- acceso al almacenamiento adicional mediante puntero explícito, sin desactivar globalmente los
  límites de arrays.

Casos SDK guía:

- `D3D12_RESOURCE_BARRIER`;
- `D3D12_CLEAR_VALUE`;
- `LARGE_INTEGER`;
- `BITMAPINFO`.

### Cierre G3a

- Layout estático y ejecución correctos.
- Comparación con Clang-cl y MSVC.
- `abi-layout-msvc-v1` certificado y archivado.

---

## Gate 3b — bitfields MSVC

La política es MSVC x64, no un algoritmo C genérico.

Implementar:

- unidad de almacenamiento según el tipo declarado;
- nueva unidad cuando cambia el tipo según las reglas MSVC;
- campos de ancho cero;
- signo y extensión;
- cruce o rechazo de límites;
- interacción con `pack`, alineación y unions.

No se usa `offsetof` sobre bitfields. El oracle:

- escribe desde C/C++ y lee desde NativeSharp;
- escribe desde NativeSharp y lee desde C/C++;
- compara bytes enmascarando bits no contractuales.

### Cierre G3b

- Casos positivos y negativos MSVC completos.
- Oracle dinámico bidireccional verde.
- `abi-bitfields-msvc-v1` certificado y archivado.

---

## Gate 3c — variádicos Win64

Pertenece a ABI de llamada, no al layout.

Implementar inicialmente sólo llamadas NativeSharp → función C/C++ variádica:

- sintaxis `extern C` con `...`;
- promociones C;
- duplicación de floats en XMM y registro entero cuando corresponde;
- shadow space;
- argumentos que rebasan registros y pasan al stack;
- diagnósticos para tipos no permitidos.

Casos:

- enteros con signo y sin signo;
- alternancia de enteros y floats;
- floats exactamente representables;
- más de cuatro argumentos;
- parámetros fijos antes de `...`.

### Cierre G3c

- Clang-cl y MSVC coinciden.
- O0/O2 y ambos linkers pasan.
- `abi-varargs-win64-v1` certificado y archivado.

---

## Gate 3d — correspondencia explícita con el Windows SDK

Añadir una representación declarativa de:

- header extranjero;
- nombre extranjero del tipo;
- nombre extranjero de cada campo;
- defines requeridos;
- arquitectura;
- familia o versión mínima del SDK.

No se introducen nombres de bibliotecas concretas dentro del compilador. D3D12, DXGI y DirectXMath
son consumidores y casos de certificación, no conocimiento especial del lenguaje.

### Cierre G3d

- Correspondencias completas y diagnósticos para mappings incompletos o ambiguos.
- El manifiesto conserva la fuente exacta de cada identidad extranjera.

---

## Gate 4 — conformidad completa contra Windows SDK

Ampliar la máquina de G2; no crear otro runner.

Nuevas acciones de `nsc`:

```text
nsc --dump-foreign-surface <proyecto.nsproj>
nsc --emit-foreign-abi-check <proyecto.nsproj> -o <destino>
```

`nsc` sólo genera archivos y manifiestos. No busca el SDK ni ejecuta Clang, MSVC o los binarios.

La certificación cubre:

- tamaños, alineaciones y offsets contra tipos reales del SDK;
- vtables mediante `offsetof(vtable_native, método) == offsetof(vtable_sdk, método)`;
- agregados de 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32, 64 y 80 bytes;
- struct vacío C++ de tamaño 1;
- floats escalares frente a agregados que contienen floats;
- `__m128` indirecto bajo la convención x64 normal;
- parámetros y retornos;
- `sret` y orden relativo con `this`;
- callbacks en ambas direcciones;
- excepciones en fronteras;
- funciones y locales sobrealineados a 32 y 64 bytes;
- stack, shadow space y registros no volátiles.

Excepciones:

- una excepción NativeSharp no cruza hacia C/C++; el thunk físico existente la contiene y termina
  de forma definida (`141` para el contrato actual);
- una excepción C++ no entra a un frame NativeSharp;
- el adaptador C++ contiene físicamente `...` o aplica una política explícita de traducción;
- `nounwind` es una licencia de optimización, nunca una barrera física.

El certificado registra:

- revisión `NSABI`;
- versión y build de nsc;
- Windows SDK, arquitectura y target triple;
- compilador C/C++ y flags;
- defines exactos;
- hash de headers/preprocesado;
- linker;
- optimización;
- digests contractual y de resultados.

### Cierre G4

- `abi-sdk-contract-v1` certificado y archivado.
- `abi-callbacks-v1` certificado y archivado.
- `abi-overaligned-v1` certificado y archivado.
- Ningún SDK ausente produce un verde: localmente es `NOT CERTIFIED`; en CI oficial es fallo.

---

## Gate 5 — CodeView/PDB mínimo utilizable

Antes del diseño completo se hace un spike con la API C de LLVM:

- `DICompileUnit`;
- `DIFile`;
- `DISubprogram`;
- ubicaciones de instrucciones;
- module flag `Debug Info Version`;
- module flag `CodeView = 1`;
- enlace `/DEBUG` y `/PDB:<ruta>`.

Separar:

- nombre de presentación: `Juego.Renderizador.Dibujar`;
- `linkageName`: símbolo interno codificado.

Auditar `.pdata` y `.xdata` en:

- funciones ordinarias y optimizadas;
- callbacks;
- `__c_boundary`;
- `__nsc_destroy_*`;
- inicializadores estáticos;
- thunks de interfaz;
- wrappers de exports/imports.

Una función leaf puede no necesitar `.pdata`. El contrato es que el stack walker atraviese el
frame correctamente, no que cada símbolo tenga una entrada propia.

### Cierre G5

- `llvm-pdbutil` demuestra módulo, función, nombre, archivo y línea.
- La entrada CodeView/RSDS del PE corresponde al PDB.
- Visual Studio o WinDbg muestran stack traces utilizables.
- lld-link y NSLink conservan la información necesaria.

---

## Gate 6 — COM y vtables extranjeras

Se separan tres contratos:

1. **COM:** ABI IUnknown, sin prometer ABI general de objetos C++.
2. **Raw foreign vtable:** tabla explícita de punteros; no promete RTTI, `delete` ni object model C++.
3. **MSVC C++ object ABI:** fuera del alcance inicial de 0.8.0-alpha.

COM implementa:

- vtable exacta y herencia de slots;
- `QueryInterface`, `AddRef` y `Release`;
- GUID de 16 bytes, constante, alineado y con dirección estable;
- `REFIID` sin temporal;
- `HRESULT` como tipo fuerte;
- COM `owned` sólo-mover;
- COM `borrowed` sin `AddRef` ni `Release`;
- copia explícita con `AddRef`;
- destrucción con `Release`;
- retorno de agregados por valor;
- objeto C++ consumido por NativeSharp;
- objeto NativeSharp consumido por C++;
- contadores que detecten fugas y dobles `Release`.

Una raw foreign vtable no promete:

- Complete Object Locator en `vftable[-1]`;
- RTTI;
- `dynamic_cast`;
- `typeid`;
- deleting destructors;
- herencia múltiple o virtual.

### Cierre G6

- Matriz COM bidireccional O0/O2 y ambos linkers.
- Retornos `sret`, `this` y slots verificados dinámicamente.
- `abi-com-v1` certificado y archivado.

---

## Gate 7 — layout constante y handles nominales

Implementar:

```csharp
sizeof(T)
alignof(T)
offsetof(T, Campo)
static_assert(condicion)
```

Todas operan en contexto constante y usan el mismo modelo físico que el lowering.

Handles fuertes:

- representación entera o puntero sin coste adicional;
- tipos nominalmente diferentes;
- comparación permitida;
- aritmética prohibida salvo conversión explícita;
- el centinela pertenece al wrapper o familia de API, no a `HANDLE` general.

`CreateFile` puede usar `-1`; otra API que devuelve `HANDLE` puede usar `NULL`. El lenguaje no
inventa un centinela universal.

### Cierre G7

- Invariantes de layout utilizables desde código NativeSharp.
- HWND/HDC/HMODULE y wrappers de handles no son intercambiables accidentalmente.
- Cero coste demostrado en HIR/LLVM y ejecución.

---

## Gate 8 — SIMD, tipos GPU y `vectorcall`

Separar dos familias:

- **almacenamiento:** structs como `XMFLOAT4`, compatibles con buffers y memoria;
- **cálculo:** vectores LLVM nativos como `<4 x float>`.

Que ambas midan 16 bytes no les concede la misma ABI.

Implementar:

- `float2`, `float3`, `float4`;
- vectores enteros;
- matrices, incluida una matriz de 64 bytes alineada a 16;
- `half` y formatos GPU empaquetados;
- swizzles y shuffles;
- `dot`, `cross`, `min`, `max`, `clamp`, `sqrt`, `rsqrt`, `fma`;
- conversiones explícitas entre almacenamiento y cálculo;
- `vectorcall`;
- clasificación HVA por tipo y posición de parámetro.

La convención x64 normal sigue pasando `__m128` indirectamente. El tipo LLVM vectorial no obliga a
usar XMM en una frontera de llamada.

El compilador implementa reglas ABI generales. Los nombres `FXMVECTOR`, `GXMVECTOR`, `HXMVECTOR` y
`CXMVECTOR` pertenecen al binding de DirectXMath y a las pruebas contra el SDK.

### Cierre G8

- Operaciones SIMD bajan a vectores/intrínsecos LLVM.
- ABI normal y `vectorcall` se distinguen correctamente.
- DirectXMath se certifica con headers reales.
- `abi-vectorcall-v1` certificado y archivado.

---

## Gate 9 — referencias sin copia y memoria colocada

Implementar:

- locales `ref`;
- retornos `ref`;
- indexadores `ref`;
- análisis de lifetime y escapes;
- placement construction;
- arenas y pools;
- tipos o contextos que prohíban heap.

Separar explícitamente:

- propiedad del lifetime del objeto;
- propiedad del almacenamiento.

Un objeto construido en una arena puede requerir destructor sin liberar individualmente la memoria
de la arena. `owned` no puede traducirse automáticamente a `free` cuando el almacenamiento pertenece
a otro owner.

### Cierre G9

- Traversal de arrays de structs sin copias.
- Ninguna referencia sobrevive a su almacenamiento.
- Placement y RAII no causan fugas ni double-free.
- O0/O2 equivalentes.

---

## Gate 10 — intrínsecos, atómicos y determinismo

Implementar intrínsecos Windows/x64:

- `popcount`;
- conteo de ceros;
- rotaciones;
- byteswap;
- `prefetch`;
- `pause`;
- `rdtsc`;
- fences;
- órdenes `relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst`.

Floating point:

- modo preciso por defecto;
- modo rápido opt-in;
- modo determinista separado;
- política explícita de FMA;
- FTZ/DAZ y redondeo;
- MXCSR por hilo;
- inicialización en workers NativeSharp;
- comprobación/restauración en fronteras extranjeras según contrato;
- biblioteca matemática fijada para operaciones trascendentales deterministas;
- CPU features y conjunto de instrucciones registrados.

### Cierre G10

- Forma LLVM/ensamblador verificada.
- Semántica de órdenes de memoria probada.
- Repeticiones deterministas bajo el hardware soportado.
- Las limitaciones frente a DLLs que alteran MXCSR están escritas y medidas.

---

## Gate 11 — PDB completo

Ampliar G5 con:

- locales;
- parámetros;
- tipos;
- structs y unions;
- handles;
- vectores y matrices;
- variables optimizadas;
- scopes;
- funciones sintetizadas claramente identificadas;
- correspondencia razonable después de optimizar.

### Cierre G11

- Inspección funcional en Visual Studio y WinDbg.
- Stack, tipos y variables se pueden consultar en código representativo del engine.
- O0/O2 y ambos linkers certificados para depuración soportada.

---

## Gate 12 — compatibilidad cruzada y certificación final

### 12.1 Matriz productor/consumidor

| Productor | Consumidor | Propósito |
|---|---|---|
| N−1 | N−1 | baseline histórico |
| N | N | baseline actual |
| N−1 | N | compatibilidad hacia atrás |
| N | N−1 | compatibilidad hacia adelante en la superficie común |

N−1 sólo tiene obligación de consumir APIs que ya existían en su superficie congelada.

### 12.2 Optimización

La optimización del productor y consumidor son ejes independientes:

- O0/O0;
- O0/O2;
- O2/O0;
- O2/O2.

### 12.3 Linkers

Para `.lib` estática se prueban ambos linkers finales.

Para DLL se prueba:

- productor lld-link → consumidor lld-link;
- productor lld-link → consumidor NSLink;
- productor NSLink → consumidor lld-link;
- productor NSLink → consumidor NSLink.

Una matriz reducida corre por gate. El producto cartesiano completo corre en nightly y release.

### 12.4 Metadata `.nsmeta`

Es un gate independiente de ejecución ABI:

- N lee N;
- N lee metadata compatible de N−1;
- N−1 lee N cuando el formato lo permite;
- major futuro se rechaza;
- minor compatible se acepta;
- fingerprint ABI incompatible se rechaza;
- revisión NSABI incompatible se rechaza;
- el rechazo ocurre antes de invocar cualquier linker;
- el diagnóstico identifica productor, consumidor, formato y revisión.

### Cierre G12

- Todos los perfiles declarados completos están certificados.
- No hay `FAILED`.
- No hay `PENDING` obligatorio.
- No hay `NOT_RUN` obligatorio.
- Clang-cl y MSVC coinciden.
- N−1/N y N/N−1 pasan para cada superficie común.
- O0/O2 y ambos linkers pasan.
- Los archivos contractuales permiten repetir la matriz.

---

## 9. Responsabilidades de cada componente

### `nsc`

- Parsear, analizar y bajar las features.
- Generar bridges y manifiestos.
- Emitir `.c/.cpp` de comprobación.
- Emitir metadata y artefactos NativeSharp.
- No buscar el Windows SDK.
- No ejecutar Clang, MSVC ni programas de certificación.

### Sistema de build

- Compilar el oracle.
- Compilar el lado NativeSharp.
- Enlazar.
- Ejecutar.
- Conservar artefactos.

### `nscbuilder.py`

- Orquestar sin BAT/CMD auxiliares.
- Mantener separados compilador y pruebas.
- Ejecutar matrices por linker y optimización.
- Capturar consolas independientes.
- Agrupar todos los errores, no sólo el primero.
- Abrir logs mediante doble clic.
- Guardar logs crudos e informes dentro del repositorio.
- Crear checkpoints y archivos contractuales cuando se solicite.
- Mostrar `CERTIFIED`, `INCOMPLETE`, `NOT CERTIFIED` o `FAILED` sin reinterpretarlos.

### CI oficial

- Exigir Windows SDK, Clang-cl y MSVC.
- Tratar la ausencia de un oracle como fallo.
- Ejecutar la matriz completa de release.
- Rechazar discrepancias entre oracles.

---

## 10. Pruebas que no pueden faltar

- Pruebas positivas y negativas por feature.
- HIR, MIR y LLVM.
- Verificación del módulo.
- Ejecución O0/O2.
- lld-link/NSLink.
- Clang-cl/MSVC.
- Mutaciones que prueben que el test realmente detecta el defecto.
- Compatibilidad N−1/N.
- `.nsmeta` compatible e incompatible.
- Stack walking y PDB.
- Ownership, destrucción, errores y excepciones.
- Valores no simétricos y patrones diagnósticos.
- Ausencia explícita de dependencias externas silenciosas.

---

## 11. Entrega final

La entrega 0.8.0-alpha contiene:

- código fuente aterrizado de todos los gates;
- compilador y runtime;
- pruebas NativeSharp y C/C++;
- oracle independiente;
- especificación de `NSABI 1`;
- perfiles ABI y sus baselines;
- archivos N−1;
- certificados;
- manifiestos y hashes;
- logs crudos;
- reportes por perfil, linker, optimización, productor y consumidor;
- documentación de sintaxis y contratos;
- lista explícita y corta de limitaciones aceptadas.

Sólo después de demostrar los criterios del Gate 12 se modifica la versión a:

```text
0.8.0-alpha
```

No se declara 0.8.0-alpha por cantidad de código escrito. Se declara cuando la ABI Windows x64,
los productos nativos, la depuración y los caminos de tiempo real incluidos en su alcance son
repetibles, diagnosticables y verificables contra autoridades independientes.
