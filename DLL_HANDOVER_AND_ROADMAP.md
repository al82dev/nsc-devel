# Handover y roadmap de DLLs NativeSharp / NativeSharp DLL handover and roadmap

**Fecha de corte / Snapshot date:** 2026-09-09
**Repositorio / Repository:** `A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls`
**Rama / Branch:** `dlls`
**HEAD verificado / Verified HEAD:** `100ce3ea9289cf40496986e2125df11e5afeb007`
**Build instalado / Installed build:** `b86d060dc8b87183`
**Formato actual / Current format:** NSMETA 1.8
**Contrato binario / Binary contract:** Native ABI 1.0 / Runtime ABI 1.0
**Plataforma / Platform:** `x86_64-pc-windows-msvc`

> Este archivo se entregó inicialmente sin commit. El usuario pidió incorporarlo al
> continuar el trabajo en la rama `dlls`.
>
> This file was initially handed over uncommitted. The user requested that it be
> incorporated when work resumed on the `dlls` branch.

## 1. Propósito / Purpose

**ES.** Este documento es el punto de entrada para continuar el carril de
bibliotecas nativas precompiladas de NativeSharp. Consolida el producto físico,
el contrato `.nsmeta`, el modelo de consumo sin fuentes, los arreglos recientes,
la evidencia y el orden recomendado de los pendientes.

**EN.** This document is the entry point for continuing NativeSharp's
precompiled native-library track. It consolidates the physical product, the
`.nsmeta` contract, source-free consumption model, recent fixes, evidence, and
recommended order for remaining work.

Este carril trata bibliotecas NativeSharp producidas por NSC. No debe confundirse
con:

- imports FFI de Windows escritos como `extern C from "user32.dll"`;
- un wrapper de carga opcional basado en `LoadLibrary`/`GetProcAddress`;
- COM, IDL o generación automática de vtables extranjeras;
- el catálogo de un binding externo.

This track covers NativeSharp libraries produced by NSC. It is distinct from
foreign Windows DLL imports, optional runtime loading, COM, and external binding
catalogs.

## 2. Reglas operativas / Operating rules

1. Trabajar en `A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls`, rama `dlls`.
2. Al corte, `dlls`, `development` y `pdb` apuntan a `100ce3ea`; no hay
   commits pendientes de traslado.
3. El contenido actual del disco es la fuente autoritativa para compilación e
   instalación rápida; no reconstruir una revisión tomada de Git.
4. Usar pruebas dirigidas durante iteración. Ejecutar el baseline una sola vez
   al cierre de un lote sustancial.
5. No convertir diferencias de C# en requisitos de NativeSharp.
6. No usar consumidores externos como criterio único del compilador: reducir
   cada defecto a un fixture propiedad de este repositorio.
7. Cada entrega futura debe incluir un MD bilingüe con archivos, funciones,
   decisiones, evidencia, límites y mensaje de commit español/inglés.
8. Este handover se crea sin commit y no autoriza cambios en otros repositorios.

## 3. Autoridades documentales / Documentary authorities

### Contrato normativo / Normative contract

- `spec/NativeSharp_Language_Specification_1_0.md`
  - `NS-LAYOUT-001`;
  - `NS-LAYOUT-002`;
  - `NS-ABI-001`;
  - `NS-ABI-002`;
  - `NS-GEN-001`.
- `spec/NativeSharp_Compiler_Contract_1_0.md`
- `include/nativesharp/Interop/LibraryMetadata.h`

### Cierre e historia de bibliotecas / Library closure and history

- `mkdown/129_cierre_native_sharp_0_9_0.md`
- `mkdown/132_1_0_e1_objetos_precompilados_completos.md`
- `mkdown/133_1_0_e2_genericos_entre_bibliotecas.md`
- `mkdown/134_1_0_e3_lifetime_delegates_excepciones_entre_modulos.md`
- `mkdown/135_1_0_e4_congelacion_abi_y_nsmeta_1_8.md`
- `mkdown/137_1_0_e5_diagnosticos_y_coherencia_abi.md`
- `mkdown/138_1_0_e6_verificacion_fisica_lib_dll.md`
- `mkdown/139_baseline_certificado_1_0_e5_e6.md`

### Arreglos posteriores / Later fixes

- `mkdown/171_1_0_1_demanda_generica_interfaces.md`
- `mkdown/172_1_0_1_origen_declarador_especializaciones_externas.md`
- `mkdown/173_1_0_1_descriptores_y_api_precompilada.md`
- `mkdown/174_1_0_1_constructores_genericos_exportados_nsmeta.md`

### Roadmap general / General roadmap

- `mkdown/121_next_steps_for_the_1_0_glory.md`, Gate 0.9;
- `mkdown/123_plan_implementacion_0_8_alpha.md`, Gate 12.

## 4. Producto físico cerrado / Closed physical product

### Biblioteca estática / Static library

```text
Producto.lib       archivo COFF con código NativeSharp
Producto.h         frontera C/C++ publicada
Producto.nsmeta    tipos, ABI, símbolos y contrato NativeSharp
```

### Biblioteca dinámica / Dynamic library

```text
Producto.dll       imagen PE32+ x64 y runtime del productor
Producto.lib       import library correspondiente
Producto.h         frontera C/C++ publicada
Producto.nsmeta    tipos, ABI, exports y contrato NativeSharp
```

Una biblioteca no es solamente un `.lib`. El metadata es obligatorio para
consumo NativeSharp porque un archive desnudo no describe tipos, ownership,
defaults, dispatch ni identidad ABI.

A library is not merely a `.lib`. Metadata is mandatory for NativeSharp
consumption because a bare archive does not describe types, ownership, defaults,
dispatch, or ABI identity.

## 5. CLI pública / Public CLI

```powershell
nsc --emit-project-oop-lib Product.nsproj -O2 -o Product.lib
nsc --emit-project-oop-dll Product.nsproj -O2 -o Product.dll
nsc --emit-project-host-header Product.nsproj -o Product.h
nsc --emit-project-library-metadata Product.nsproj -o Product.nsmeta

nsc --verify-library-metadata Product.nsmeta
nsc --verify-library-artifacts Product.nsmeta Product.lib
nsc --verify-library-artifacts Product.nsmeta Product.lib Product.dll
```

Las emisiones de `.lib` y DLL generan sus productos acompañantes según el
contrato del driver. Las órdenes explícitas de header y metadata permiten además
inspección o regeneración aislada.

Static-library and DLL emission produce their companion products under the
driver contract. Explicit header and metadata commands also allow isolated
inspection or regeneration.

## 6. Consumo mediante LibraryReference / Consumption through LibraryReference

### Estática / Static

```xml
<LibraryReference Include="../Producer/Product.lib"
                  Metadata="../Producer/Product.nsmeta"/>
```

Si `Metadata` se omite, NSC deriva el mismo basename con extensión `.nsmeta`.
El archivo debe existir.

When `Metadata` is omitted, NSC derives the same basename with a `.nsmeta`
extension. The file must exist.

### DLL

```xml
<LibraryReference Include="../Producer/Product.lib"
                  Metadata="../Producer/Product.nsmeta"
                  Runtime="../Producer/Product.dll"/>
```

- `Include`: `.lib` estática o import library.
- `Metadata`: `.nsmeta`, explícita o derivada.
- `Runtime`: DLL opcional; cuando existe, activa las comprobaciones de producto
  dinámico y se propaga al consumidor final.
- Las rutas se resuelven respecto al proyecto y se canonicalizan.
- Referencias duplicadas y extensiones incorrectas se rechazan temprano.

## 7. NSMETA 1.8 / NSMETA 1.8

### Evolución aditiva / Additive evolution

| Minor | Información agregada / Added information |
|---:|---|
| 1.1 | ownership en campos, miembros y parámetros |
| 1.2 | tipo subyacente de enums |
| 1.3 | valores predeterminados |
| 1.4 | modo de parámetro Value/ref/out/in |
| 1.5 | firma de delegate y distinción `extern C` |
| 1.6 | accessor PE-safe del descriptor de clase |
| 1.7 | declaración canónica de templates genéricos públicos |
| 1.8 | contrato ABI explícito y representación de delegates ordinarios |

El lector actual acepta NSMETA 1.0 a 1.8 conforme a las reglas históricas. No
inventa capacidades que un minor anterior nunca publicó. Un major distinto o un
minor futuro se rechaza antes del enlace.

The current reader accepts NSMETA 1.0 through 1.8 under the historical rules. It
does not invent capabilities that an older minor never published. A different
major or future minor is rejected before linking.

### Contrato físico congelado / Frozen physical contract

| Componente / Component | Valor / Value |
|---|---:|
| Native ABI | 1.0 |
| Runtime ABI | 1.0 |
| Mangling ABI | 1 |
| Object ABI | 1 |
| Delegate ABI | 1 |
| Exception ABI | 1 |
| Managed delegate cell | 32 bytes |
| Pointer size/alignment | 8/8 |

La huella SHA-256 cubre módulo, target, tipos, layouts, firmas, ownership,
defaults, símbolos, dispatch, templates, especializaciones y contratos físicos.
`producerVersion` identifica el build, pero no altera por sí sola la ABI.

## 8. Arquitectura de productor y consumidor / Producer-consumer architecture

```text
Fuentes del productor
   │
   ├─ frontend + HIR/MIR + LLVM
   ├─ .lib o DLL/import .lib        cuerpos y símbolos físicos
   ├─ .h                            frontera C estable
   └─ .nsmeta 1.8                   contrato NativeSharp
                                      │
                                      ▼
LibraryReference del consumidor
   │
   ├─ valida metadata y ABI
   ├─ reconstruye sólo API publicable
   ├─ hidrata templates públicos
   ├─ reutiliza especializaciones publicadas
   ├─ monomorfiza especializaciones nuevas
   ├─ enlaza símbolos externos exactos
   └─ valida archive/DLL antes y después del enlace
```

El consumidor no comparte fuentes ordinarias del productor ni regenera sus
cuerpos. Las únicas implementaciones transportadas como texto canónico son
templates genéricos públicos materializables.

The consumer shares no ordinary producer sources and does not regenerate their
bodies. Public materializable generic templates are the only implementations
transported as canonical declaration text.

## 9. Superficie implementada / Implemented surface

| Capacidad / Capability | Estado |
|---|---|
| `.lib` COFF estática | Cerrada |
| DLL PE32+ e import `.lib` | Cerrada |
| Header C/C++ determinista | Cerrada |
| NSMETA determinista y autoverificable | Cerrada |
| Funciones estáticas, overloads y defaults | Cerrada |
| Value/ref/out/in | Cerrada |
| Structs naturales, packed, aligned y anidados | Cerrada |
| Enums, signo, ancho, underlying type y constantes | Cerrada |
| Métodos de instancia de structs | Cerrada |
| Clases, construcción, herencia y base precompilada | Cerrada |
| Métodos no virtuales, virtuales e interfaces | Cerrada |
| Descriptor, vtable, interface tables y destruction entry | Cerrada |
| Owned/borrowed/move y cleanup del productor | Cerrada |
| Delegate `extern C` | Cerrada |
| Delegate NativeSharp, estático, instancia y closure | Cerrada |
| Excepciones tipadas entre módulos | Cerrada |
| Templates públicos y especializaciones importadas/nuevas | Cerrada |
| Constraints genéricos | Cerrada |
| Dependencias transitivas y múltiples bibliotecas | Cerrada para fixtures actuales |
| Diagnósticos ABI por componente | Cerrada |
| Verificación conjunta `.nsmeta` + `.lib` + DLL | Cerrada |
| Matriz completa N−1/N y cuatro combinaciones de linker | Pendiente de certificación final |
| PDB para DLL y biblioteca | Pendiente |

## 10. Las cuatro correcciones recientes / The four recent fixes

### `2d6cbfef` — demanda genérica desde interfaces

Una tabla de interfaz es una referencia ejecutable. Sus implementaciones
genéricas cerradas se agregan a la demanda de cuerpos aunque no exista una
llamada sintáctica directa. Esto elimina:

```text
Stage 36 concrete interface implementation has no OOP HIR function
```

### `1257ce07` — origen declarador de argumentos externos

Una primera especialización como `Dictionary<string, Texture2D>` conserva el
proyecto y namespace donde se declaró `Texture2D`, no el sitio físico del clon
genérico. Se cubren clases, structs, interfaces, tipos anidados y referencias
transitivas.

### `e84ca319` — descriptores y API precompilada

- las bases precompiladas se obtienen mediante `__nsc_get_classdesc_*`, no
  tratando una celda IAT como dato;
- descendientes locales publican la base una sola vez con `cmpxchg`;
- un `extern C` no exportado sigue siendo dependencia privada del productor y no
  se reconstruye como método NativeSharp público.

### `809e77be` — constructores genéricos y verificación física

`publishedNativeSymbols()` separa ahora:

1. qué constructor publica físicamente el productor;
2. qué constructor puede reconstruir el importador como API utilizable por
   `new`.

Un constructor público de struct genérico concreto debe aparecer en DLL y
NSMETA, aunque el consumidor todavía no pueda materializar `new` sobre ese
struct precompilado.

## 11. Distinciones que deben conservarse / Distinctions to preserve

### Publicación física no es materialización de fuente

La tabla PE y el archive describen lo que existe. La API sintética describe lo
que el consumidor puede usar de forma segura. No reutilizar una sola función de
predicado para ambas preguntas.

### `extern C` no es `export extern C`

Un import nativo del productor puede ser público en sintaxis sin ser parte del
producto NativeSharp. Sólo `export extern C` publica el thunk C y la entrada
NativeSharp contractual.

### Descriptor de datos no es accessor PE

Una DLL introduce indirección para imports de datos. Los consumidores obtienen
el descriptor real mediante la función accessor publicada.

### Template no es especialización

- una especialización publicada se reutiliza por clave canónica;
- una combinación nueva se materializa en el consumidor desde la declaración
  pública;
- el consumidor no debe fingir que la combinación nueva ya vive en la DLL.

### Identidad no es spelling

El texto visible puede conservar calificadores útiles, pero la clave persistente
usa identidad canónica de módulo y argumentos. No volver a lookup por texto
perdido dentro de un clon.

## 12. Código crítico / Critical code

### Driver e importador

- `src/Driver/Driver.cpp`
  - `buildPrecompiledPublicSource`;
  - `buildPrecompiledGenericTemplateSource`;
  - `configurePrecompiledModules`;
  - `appendValidatedLibraryReferences`;
  - `canMaterializePrecompiledDeclaration`;
  - `canMaterializeConstructor`;
  - `isPublishedNativeConstructor`;
  - `publishedNativeSymbols`;
  - comandos de emisión y verificación.

### Metadata

- `include/nativesharp/Interop/LibraryMetadata.h`
- `src/Interop/LibraryMetadata.cpp`
  - codec;
  - checksum;
  - ABI projection/fingerprint;
  - `verifyFrozenAbiContract`;
  - `verifyCurrentCompiler`.

### Proyecto, frontend y genéricos

- `include/nativesharp/Compilation/Project.h`
- `src/Compilation/ProjectLoader.cpp`
- `src/Compilation/ProjectGraph.cpp`
- `include/nativesharp/Frontend/ProjectCompilation.h`
- `include/nativesharp/Frontend/ProjectGenericModel.h`
- `src/Frontend/ProjectGenericModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`

### OOP lowering y producto

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/Linker/WindowsLibraryBuilder.cpp`
- `src/Linker/WindowsExecutableBuilder.cpp`
- `src/Linker/LldLinker.cpp`

## 13. Gates que no deben romperse / Gates that must not regress

- `nsc.stage46.native-libraries`
  - producción y consumo estático/DLL;
  - ejecución O0/O2;
  - tipos, objetos, genéricos, delegates, excepciones y transitividad.
- `nsc.stage48.1-c-header`
- `nsc.stage48.2-library-metadata`
- `nsc.stage48.3-c-host`
- `nsc.stage48.4-cpp-host`
- `nsc.stage48.5-csharp-host`
- `nsc.stage48.6-cppcli-host`
- suites Stage 44, Stage 45 y Stage 50 relacionadas con genéricos,
  excepciones y ownership.

## 14. Evidencia del último punto estable / Last stable-point evidence

### Arreglos DLL/NSMETA dirigidos

- Stage 44 generics: 75/75;
- Project OOP lowering: 71/71;
- Stage 52 external generic origin: 147/147;
- Stage 46 native libraries: 145/145;
- Stage 48.2 library metadata: 98/98;
- consumidor Core mínimo: compile 0, run 42;
- repro `precompiled_dll_generic_exports`: `BUILD=0 VERIFY=0`,
  `runtime-exports=3`, `required-symbols=3`.

La evidencia externa reportada por el binding alcanzó además enlace y ejecución
D3D11 con resultado 42. Debe tratarse como corpus consumidor, no como sustituto
de los gates propiedad de NSC.

### Baseline global sobre el mismo HEAD

- corrida: `A:\nsc-devel\_nscbuilder\history\20260909-155950-ff92fdc8`;
- build instalado: `b86d060dc8b87183`;
- CTest lld-link: 307/307;
- fixtures lld-link: 148/148;
- triage lld-link: 18/18;
- CTest NSLink: 307/307;
- fixtures NSLink: 148/148;
- triage NSLink: 18/18;
- Gate 0: `LANDED`;
- duración: 1541.36 s.

El baseline fue ejecutado desde `A:\nsc-devel`, pero esa rama y `dlls` apuntaban
al mismo commit `100ce3ea` al crear este handover.

## 15. Roadmap de continuación / Continuation roadmap

### DLL-P0 — Congelar un corpus mínimo de los defectos externos

**Estado / Status:** cerrado en la continuación documentada por
`mkdown/175_1_0_1_corpus_regresiones_dll_precompiladas.md`.

**Closed:** the internal corpus now builds and jointly verifies two DLL/import
library/NSMETA products, consumes them without producer sources, and executes to
42. The source fixture retains nested and transitive declaration-origin coverage;
the DLL fixture adds class, struct, and interface arguments from a second metadata
producer.

**Objetivo / Goal:** asegurar que los cuatro arreglos recientes no dependan de
Saru, DirectX o bindings completos.

- confirmar que cada defecto externo tiene repro mínimo dentro de tests;
- mantener el caso de constructor de struct genérico en Stage 48.2;
- ampliar el fixture de origen declarador con clase, struct, interfaz, nested y
  transitividad;
- agregar un producto mínimo con dependencia nativa privada no exportada;
- exigir compilación, verificación de artefactos y ejecución del consumidor.

**Cierre / Exit:** borrar el consumidor externo no reduce la cobertura del
compilador.

**Commit sugerido / Suggested commit:**
`Consolida las regresiones de DLLs precompiladas / Consolidate precompiled DLL regressions`

### DLL-P1 — Matriz cruzada de productor y consumidor por linker — CERRADO / CLOSED

El roadmap exige cuatro combinaciones dinámicas:

1. productor lld-link → consumidor lld-link;
2. productor lld-link → consumidor NSLink;
3. productor NSLink → consumidor lld-link;
4. productor NSLink → consumidor NSLink.

Agregar O0/O2 como ejes independientes del productor y consumidor. La matriz
reducida corre en cada gate; el producto cartesiano completo queda para nightly
y release.

**Cierre / Exit:** cada combinación construye DLL/import library/NSMETA, verifica
pertenencia contractual y ejecuta 42.

**Commit sugerido / Suggested commit:**
`Certifica la matriz cruzada de DLLs y linkers / Certify the cross-linker DLL matrix`

### DLL-P2 — Compatibilidad NSMETA N−1/N — CERRADO / CLOSED

- N lee N;
- N lee cada minor histórico soportado;
- N−1 lee N sólo cuando el formato común lo permite;
- major y minor futuros se rechazan;
- fingerprint, Native ABI o Runtime ABI incompatibles se rechazan;
- el diagnóstico identifica productor, consumidor, formato y componente;
- conservar goldens reales, no sólo modelos construidos en memoria.

**Cierre / Exit:** la matriz usa binarios de compilador independientes y
artefactos archivados.

**Commit sugerido / Suggested commit:**
`Certifica compatibilidad cruzada de NSMETA / Certify cross-version NSMETA compatibility`

### DLL-P3 — Genéricos precompilados completos — CERRADO / CLOSED

- templates de tipo y método;
- especialización publicada frente a nueva;
- constraints con clase, struct e interfaz;
- argumentos declarados en tercero y visibles transitivamente;
- genéricos anidados;
- implementaciones virtuales/de interfaz demandadas sólo por tablas;
- defaults, overloads y ownership dentro de firmas genéricas;
- ambigüedad entre módulos homónimos;
- rechazo de template incompleto o dependencia privada arrastrada.

**Cierre / Exit:** el productor publica sólo el contrato necesario y el
consumidor monomorfiza con el mismo modelo que una compilación local.

**Commit sugerido / Suggested commit:**
`Endurece genéricos entre DLLs NativeSharp / Harden generics across NativeSharp DLLs`

### DLL-P4 — Objetos, dispatch y lifetime entre módulos — CERRADO / CLOSED

- cadenas de herencia mixtas local/precompilada;
- descriptor accessor con carreras de inicialización;
- vtables e interfaces con múltiples implementaciones;
- constructors/base initializers;
- destrucción exactamente una vez;
- owned/borrowed/move en parámetros, retornos y campos;
- delegates de método estático, instancia y closure;
- throw/catch/finally atravesando DLL;
- múltiples runtime DLLs sin colisión de identidad.

**Cierre / Exit:** dispatch y cleanup conservan identidad del productor en O0/O2
y bajo ambos linkers.

**Commit sugerido / Suggested commit:**
`Amplía objetos y lifetime entre DLLs / Expand objects and lifetime across DLLs`

### DLL-P5 — Dependencias dinámicas y empaquetado — CERRADO / CLOSED

- una DLL conserva sus imports Win32/nativos privados;
- el consumidor no redeclara ni enlaza dependencias privadas del productor;
- las DLL NativeSharp transitivas se identifican por módulo y contrato;
- colisiones de basename o contratos que reclaman DLL distinta fallan temprano;
- el paquete coloca runtimes requeridos sin escanear rutas arbitrarias;
- diagnóstico para runtime ausente, DLL equivocada y arquitectura errónea;
- rutas Unicode y carga fuera del checkout.

No reabrir `NativeLibrary` sólo para resolver empaquetado. La carga opcional en
runtime sigue fuera del roadmap salvo un caso medido nuevo.

**Commit sugerido / Suggested commit:**
`Formaliza dependencias y empaquetado de DLLs / Formalize DLL dependencies and packaging`

### DLL-P6 — Verificación física y robustez negativa — CERRADO / CLOSED

- archive truncado, índice corrupto y miembro ilegible;
- PE que no es DLL o tiene máquina incorrecta;
- import library que pertenece a otra DLL;
- exports faltantes e inesperados;
- metadata con checksum válido pero tablas incoherentes;
- límites de tamaño, recursión, conteos y strings antes de reservar;
- dos módulos con misma identidad;
- dos exports C duplicados;
- residuos parciales eliminados después de un fallo.

El verificador no debe ejecutar código ni confiar en extensiones.

**Commit sugerido / Suggested commit:**
`Endurece la validación de artefactos NativeSharp / Harden NativeSharp artifact validation`

### DLL-P7 — PDB para bibliotecas

El carril PDB actual cubre objetos y ejecutables mediante lld-link. Falta:

- PDB de la DLL del productor;
- separación de fuentes del productor y consumidor;
- stack mixto EXE→DLL→callback→EXE;
- tipos y funciones sintetizadas de cada módulo;
- GUID/age y localización del PDB por módulo;
- O0/O2 y, cuando exista escritor PDB, paridad NSLink.

Coordinar con `A:\NSC-DEVEL-BRANCHES\nsc-support-pdb` sin mezclar ambos lotes en
un solo commit.

**Commit sugerido / Suggested commit:**
`Agrega PDB a productos DLL NativeSharp / Add PDB support to NativeSharp DLL products`

### DLL-P8 — Hosts externos y ABI pública

- C, C++, C# P/Invoke y C++/CLI;
- structs por valor e indirectos;
- callbacks y exception boundary;
- bool, enums, punteros, handles y strings C explícitas;
- overloads mediante nombres C publicados;
- headers compilados con clang-cl y MSVC;
- productor/host con optimizaciones independientes.

No exponer clases NativeSharp como ABI C++ implícita. El header publica la
frontera C que puede expresar y verificar.

**Commit sugerido / Suggested commit:**
`Certifica hosts externos para DLLs NativeSharp / Certify external hosts for NativeSharp DLLs`

### DLL-P9 — Certificación final

- producto estático y dinámico;
- O0/O2 productor y consumidor;
- ambos linkers y matriz cruzada;
- metadata histórica y actual;
- transitividad;
- genéricos, objetos, interfaces, delegates y excepciones;
- hosts externos;
- PDB por módulo;
- instalación limpia;
- repetibilidad y hashes;
- G00 una sola vez al cierre.

**Cierre final / Final exit:** productor y consumidor separados funcionan sin
fuentes compartidas, los artefactos se verifican independientemente, los
rechazos ocurren antes del linker cuando corresponde y el producto instalado es
exactamente el certificado.

## 16. Orden recomendado / Recommended order

```text
DLL-P0 regresiones propias
   ↓
DLL-P1 matriz cruzada de linkers
   ↓
DLL-P2 compatibilidad NSMETA
   ├─ DLL-P3 genéricos
   ├─ DLL-P4 objetos/lifetime
   └─ DLL-P5 dependencias/paquete
          ↓
DLL-P6 robustez física
   ├─ DLL-P7 PDB por módulo
   └─ DLL-P8 hosts externos
          ↓
DLL-P9 certificación final
```

El primer siguiente cambio recomendado es DLL-P0: convertir todo hallazgo del
binding en evidencia mínima propiedad del compilador. Después, DLL-P1 cierra la
promesa más fácil de sobreestimar hoy: que “ambos linkers pasan” equivale a haber
probado las cuatro combinaciones productor/consumidor.

## 17. Secuencia de verificación / Verification sequence

Durante iteración:

1. construir sólo `nsc` y suites afectadas;
2. ejecutar Stage 46 y/o Stage 48.2;
3. ejecutar suites semánticas relacionadas;
4. verificar `.nsmeta`, `.lib` y DLL por separado;
5. ejecutar consumidor O0/O2;
6. repetir con el compilador instalado al cerrar el arreglo.

Al cierre de un lote sustancial:

```powershell
python A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\nscbuilder.py --baseline --jobs 4
```

No ejecutar el baseline después de cada microcambio.

## 18. Riesgos conocidos / Known risks

- `buildPrecompiledPublicSource` genera sintaxis interna; cambios de parser deben
  mantener metadata histórica.
- Una declaración sintética con cuerpo vacío no es implementación del productor.
- El source slice de templates es contrato público; no debe filtrar privados.
- Resolver tipos sustituidos sólo por texto rompe argumentos de un tercer módulo.
- Tratar datos importados de PE como funciones, o viceversa, rompe descriptores.
- Debilitar igualdad exacta de exports DLL oculta superficie no descrita.
- Exigir igualdad exacta de todos los símbolos del archive sería incorrecto:
  helpers internos adicionales son válidos.
- Un baseline con ambos linkers no demuestra por sí solo la matriz cruzada.
- Un binding grande verde ayuda, pero no sustituye un repro pequeño en NSC.

## 19. Primer paso concreto / Concrete first step

Inventariar los repros externos que originaron `2d6cbfef`, `1257ce07`,
`e84ca319` y `809e77be`; mapear cada uno a un caso interno y completar cualquier
hueco sin ejecutar el baseline. El lote dirigido debe cerrar con:

1. Stage 44;
2. Stage 46;
3. Stage 48.2;
4. productor DLL;
5. `--verify-library-artifacts`;
6. consumidor que devuelve 42;
7. compilador instalado desde el disco.

Después de ese cierre dirigido, comenzar DLL-P1.
