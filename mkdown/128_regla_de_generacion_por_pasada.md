# 128 — Bitácora didáctica y regla de generación por pasada

**Vigencia:** permanente desde `nsc 0.9.0-alpha`.

Este documento cumple dos funciones inseparables:

1. explicar qué problema resolvió cada pasada de `0.9`, cómo lo resolvió y qué prueba evita que
   reaparezca;
2. exigir que cada una de esas pasadas termine materializada en un `nsc.exe` nuevo.

Una lista de archivos modificados no explica la evolución del compilador. La unidad de relato es:
**síntoma → causa → corrección → garantía → evidencia**.

## Granularidad de una entrega

Los átomos son unidades internas de implementación y prueba; no son, por sí solos, entregas para
el usuario. Una entrega ordinaria debe cerrar una rebanada vertical utilizable y reunir varios
átomos relacionados.

Por ejemplo, «leer un registro nuevo de metadata» no constituye una entrega. El cierre llega
cuando ese registro entra por el proyecto, participa en resolución semántica, llega al backend,
enlaza, ejecuta, tiene negativos precisos, queda documentado y existe un `nsc.exe` nuevo que lo
contiene.

Cada entrega debe incluir, salvo que se trate de una corrección urgente y explícitamente aislada:

1. modelo o formato necesario;
2. integración con el frontend y diagnóstico;
3. lowering/enlace cuando corresponda;
4. caso positivo de productor y consumidor separados;
5. casos negativos que demuestren el límite exacto;
6. ejecución real del producto generado;
7. matriz de regresión proporcional;
8. generación de `nsc.exe`, identidad y SHA-256;
9. actualización didáctica de la bitácora.

Los avances parciales pueden comunicarse durante el trabajo para hacer visible un hallazgo o un
bloqueo, pero no se presentan como cierre ni sustituyen la entrega acumulada.

### Entrega vertical de consumo semántico

El corte comprometido ya está implementado para métodos NativeSharp estáticos públicos con tipos
base, además de la superficie pública `export extern C`:

- `LibraryReference` carga su metadata dentro de la compilación;
- cada `.nsmeta` se representa mediante un módulo precompilado de identidad canónica;
- sus tipos declarantes, métodos NativeSharp y overloads entran en el frontend ordinario;
- una llamada se resuelve sin declaración `extern C` manual en el consumidor;
- se conservan nombres de parámetros, ownership, defaults y `ref`/`out`/`in`;
- la misma llamada enlaza mediante biblioteca estática o DLL/import library;
- miembros no publicados, módulos ambiguos y contratos incompatibles se rechazan;
- productores y consumidores separados ejecutan el resultado real.

No se marca aquí como resuelto el consumo de tipos nominales completos, miembros de instancia,
genéricos o dispatch. Esos elementos siguen siendo parte del gate amplio de `0.9`; esta entrega
cierra una rebanada usable sin atribuirle capacidades que todavía no posee.

## Mapa mínimo de los productos

Antes de recorrer las correcciones conviene separar tres archivos que en Windows comparten parte
del vocabulario, pero no cumplen la misma función:

| Producto | Contenido | Papel |
|---|---|---|
| `.lib` estática | Objetos COFF con implementación | El código se incorpora al binario final |
| import `.lib` | Índice y stubs para símbolos de un DLL | Permite enlazar llamadas que ejecutará el DLL |
| `.nsmeta` | Contrato semántico y ABI de NativeSharp | Permite validar qué módulo, tipos y firmas publica el binario |

Que un archivo termine en `.lib` no demuestra que sea la biblioteca correcta. Que un DLL exporte
un símbolo llamado `Add` tampoco demuestra que su firma coincida con la declaración del
consumidor. El trabajo de `0.9` consiste precisamente en unir esas identidades y rechazar las
combinaciones falsas antes de ejecutarlas.

## Pasadas resueltas en `0.9.0-alpha`

### Pasada 1 — identidad de versión de la nueva línea

**Síntoma:** el trabajo ya pertenecía a la frontera de bibliotecas de `0.9`, pero el compilador y
sus pruebas de identidad todavía describían la línea anterior.

**Corrección:** la versión de CMake, la versión pública del compilador y la prueba de identidad se
movieron conjuntamente a `0.9.0-alpha`. El sufijo `+build.<UTC>.<hash>` continúa distinguiendo cada
ejecutable construido sin convertir cada build en una ABI diferente.

**Garantía:** el artefacto dice qué línea de compatibilidad implementa y qué construcción exacta
lo produjo.

### Pasada 2 — `NSMETA 1.3` conserva argumentos predeterminados

**Síntoma:** una API compilada desde fuentes conocía los defaults de sus parámetros, pero al
publicarse como biblioteca el metadata sólo retenía nombre, tipo y ownership. El consumidor
precompilado veía un contrato incompleto.

**Causa:** `NSMETA 1.2` no tenía representación para valores predeterminados.

**Corrección:** `NSMETA 1.3` representa explícitamente:

- ausencia de default;
- `null`;
- booleanos;
- enteros con signo separado de la magnitud;
- flotantes mediante sus bits IEEE-754;
- caracteres;
- cadenas.

Los defaults participan en la huella ABI. Cambiar un default cambia el contrato publicado aunque
la firma máquina conserve los mismos parámetros.

**Compatibilidad:** el lector actual sigue aceptando `NSMETA 1.0`, `1.1` y `1.2`. Para un archivo
antiguo no inventa un default: trata el parámetro como obligatorio.

**Evidencia:** round-trip de cada clase de literal, sensibilidad de la huella y lectura alineada de
un archivo `1.2` real generado por la prueba.

### Pasada 3 — `LibraryReference` convierte la biblioteca en entrada de proyecto

**Síntoma:** `nsc` podía producir DLL y `.lib`, pero otro proyecto sólo podía introducirlos con
argumentos sueltos del driver. El proyecto no describía el vínculo entre binario, metadata y DLL.

**Corrección:** el formato `.nsproj` admite:

```xml
<LibraryReference Include="../Math/NativeMath.lib"
                  Metadata="../Math/NativeMath.nsmeta"
                  Runtime="../Math/NativeMath.dll" />
```

- `Include` identifica la `.lib` estática o de importación;
- `Metadata` identifica el contrato; si se omite, usa el mismo nombre base con extensión
  `.nsmeta`;
- `Runtime` identifica el DLL correspondiente y sólo es necesario para producto dinámico.

Las rutas se resuelven respecto al proyecto que declara la referencia, se canonicalizan, se
validan y se deduplican. El driver incorpora la `.lib` automáticamente al enlace de EXE o DLL.

**Garantía:** la dependencia binaria deja de ser conocimiento escondido en un script de build y
forma parte del proyecto reproducible.

### Pasada 4 — validación física de biblioteca, metadata y DLL

**Síntoma:** una ruta existente no garantiza que el archivo tenga el formato esperado ni que
pertenezca al target y versión del consumidor.

**Corrección:** antes del enlace, `nsc` verifica:

- estructura de archive COFF de la `.lib`;
- versión de `NSMETA` soportada;
- target, tamaño y alineación de puntero;
- versión productora compatible;
- identidad de módulo no vacía y no duplicada;
- estructura PE del DLL;
- igualdad exacta entre exports del DLL y exports publicados en metadata.

Después del enlace inspecciona el PE final y confirma que el import `.lib` hizo importar el nombre
de DLL declarado en `Runtime`.

**Problema concreto cerrado:** dos DLL podían publicar la misma lista de símbolos y, aun así, el
import `.lib` pertenecer al DLL equivocado. Comparar sólo las tablas de exports no lo detectaba. La
comprobación del PE final sí distingue qué DLL quedó realmente enlazado.

### Pasada 5 — la declaración consumidora no puede mentir sobre la ABI

**Síntoma:** el metadata podía decir `int Add(int, int)`, mientras el consumidor redeclaraba
`long Add(int, int)`. Ambos lados compilaban y el enlazador sólo veía el mismo nombre `Add`.

**Causa:** un enlazador nativo resuelve símbolos; no conoce los tipos de NativeSharp que originaron
la llamada.

**Corrección:** para cada `extern C` que coincide con un export de una `LibraryReference`, `nsc`
compara el tipo de retorno y todos los tipos de parámetros contra `.nsmeta`. Si la importación
nombra un DLL, la selección del contrato también usa esa identidad de runtime.

**Garantía:** una firma ABI incompatible produce error de compilación indicando símbolo y módulo,
antes de crear el ejecutable. Las bibliotecas externas que no poseen `.nsmeta` conservan su camino
de interoperabilidad existente.

### Pasada 6 — el `.lib` debe contener los símbolos prometidos

**Síntoma:** una `.lib` estructuralmente válida podía acompañarse accidentalmente con el
`.nsmeta` de otro producto. El metadata era válido y el archive también, pero la pareja era falsa.

**Corrección:** el verificador COFF ahora lee el primer linker member del archive, interpreta su
tabla big-endian y obtiene el índice real de símbolos. Cada export C publicado por `.nsmeta` debe
existir exactamente en ese índice.

**Garantía:** el error aparece como incompatibilidad biblioteca/metadata y señala el primer símbolo
prometido que falta, en vez de convertirse más tarde en un error de enlace sin contexto.

### Pasada 7 — símbolos RTTI privados entre módulos

**Síntoma:** al enlazar un ejecutable con una estática NativeSharp aparecieron definiciones fuertes
duplicadas de `__nativesharp_rtti_is_class` y `__nativesharp_rtti_is_interface`.

**Causa:** cada módulo emitía sus auxiliares RTTI con linkage externo, aunque eran detalles de
implementación locales al módulo.

**Corrección:** ambos helpers se emiten con linkage interno de LLVM. La declaración mínima de
`LLVMSetLinkage` se añadió a la capa C usada por el backend.

**Garantía:** varios módulos pueden llevar su propia implementación RTTI sin exportar ni colisionar
esos nombres en el enlace final.

### Pasada 8 — composición de bibliotecas estáticas

**Síntoma inicial:** una estática A podía llamar a una estática B y validar su
`LibraryReference`, pero A no incorporaba B. El consumidor necesitaba conocer y enlazar también B,
aunque la dependencia perteneciera a la implementación privada de A.

**Primera corrección:** al producir A, el librarian consolida dentro de su archive los objetos de
las `.lib` estáticas referenciadas.

**Segundo síntoma descubierto por la prueba:** A ya contenía el código de B, pero el enlace del
consumidor falló con `undefined symbol: __C_specific_handler`.

**Causa:** el consumidor sólo puede inspeccionar su propio IR. No puede redescubrir imports de
soporte requeridos por IR que ya vive compilado dentro de A o B.

**Corrección de 0.9:** la estática consolidada incorpora los objetos de sus dependencias, pero no
las import libraries del proceso. Los imports de runtime y soporte del compilador se materializan
una sola vez en el enlace final. Esta precisión reemplaza la primera solución, que sólo era segura
con la selección perezosa de archives de lld-link.

**Garantía:** la prueba construye B, construye A dependiendo de B y después construye un ejecutable
que referencia únicamente A. El índice de A contiene `Answer` y `Add`; el ejecutable corre y
devuelve `42`.

### Pasada 9 — `.nsmeta` entra en la semántica del consumidor

**Síntoma:** el proyecto ya localizaba y validaba la `.lib`, el DLL y el `.nsmeta`, pero el
programador todavía debía copiar una declaración `extern C` en el consumidor. Esa copia repetía
nombres y tipos que ya estaban en metadata y permitía que la experiencia fuente y la binaria
divergieran.

**Causa:** `LibraryReference` sólo participaba en validación y enlace. El grafo y el frontend no
tenían una unidad que representara el módulo precompilado.

**Corrección:** cada metadata aporta un nodo canónico al `ProjectGraph`. A partir de su superficie
pública `export extern C`, el driver materializa declaraciones que el `SourceManager` entrega al
mismo frontend usado por los archivos `.ns`. La unidad queda marcada como precompilada: participa
en lookup y resolución de llamadas, pero no vuelve a generar el código, RTTI ni dispatch que ya
pertenecen al productor.

Los parámetros se reconstruyen con nombre, tipo, ownership y valor predeterminado. Así,
`Precompiled.Math.Add(left: 20)` usa el default `right = 22`, enlaza tanto contra estática como
contra DLL y devuelve `42` en ejecución real.

**Negativos:** un miembro privado no aparece en el contrato importado; dos bibliotecas que
declaran la misma identidad canónica producen ambigüedad; una firma manual incompatible sigue
siendo rechazada, ahora incluso puede detectarse durante la agregación con la declaración
importada.

**Garantía:** el contrato semántico y el binario ya recorren una sola tubería. La metadata no crea
un lookup privilegiado que pudiera contradecir al frontend.

### Pasada 10 — símbolos NativeSharp, overloads y NSMETA 1.4

**Síntoma:** la primera importación semántica seguía apoyándose en `export extern C`. Eso permitía
llamar un método sencillo, pero el nombre C no representa overloads y obligaba a que una DLL
NativeSharp publicara una fachada de host aunque sólo fuera a consumirla otro módulo NativeSharp.

**Causa:** el DLL sólo exportaba wrappers C y la unidad precompilada se bajaba como import C. La
metadata ya guardaba `mangledName`, pero el frontend no convertía una declaración importada en una
referencia a ese símbolo. Además, `NSMETA 1.3` guardaba el carrier puntero de `ref`/`out`/`in`, no
cuál de las tres reglas fuente lo produjo.

**Corrección:** una unidad `.nsmeta` declara métodos NativeSharp ordinarios. Participan en lookup,
argumentos nombrados, defaults y selección de overload; al llegar a HIR se transforman en una
declaración externa con el mangling calculado desde la identidad canónica del productor. MIR y
LLVM conservan la declaración sin generar una segunda implementación.

El productor DLL exporta cada método estático público mediante su símbolo mangled y conserva, para
los miembros `export extern C`, el wrapper C estable adicional. Una DLL con API exclusivamente
NativeSharp es ahora válida. La validación exige que cada símbolo prometido exista tanto en el
índice COFF como, para DLL, en la tabla PE de exports.

`NSMETA 1.4` añade `Value`, `ref`, `out` e `in` al registro de parámetro y a la huella del contrato.
El escritor sólo añade ese byte en 1.4; el lector de 1.3 toma `Value` y conserva alineado el default
que viene después. Los consumidores estático y dinámico ejercitan los tres modos en ejecución.

**Negativo:** una import library que posee el mismo nombre C pero no los símbolos mangled del
módulo ya no puede hacerse pasar por la biblioteca correcta. El diagnóstico identifica el primer
símbolo NativeSharp ausente antes de invocar al enlazador.

### Pasada 11 — linker y librarian dejan de ser la misma herramienta

**Síntoma certificado:** el gate `build-test-nslink -j 16` dejó seis suites de bibliotecas en rojo.
La primera creación de cada `.lib` fallaba; los errores posteriores de referencias inexistentes
eran sólo cascada.

**Causa reproducida:** `NSC_LLD_LINK=nslink.exe` seleccionaba correctamente NSLink para PE, pero
`LldLinker::createStaticLibrary` reutilizaba ese mismo ejecutable con `/lib`. NSLink no es un
librarian: intentó leer una import library como objeto y diagnosticó `NSL1003`, máquina no AMD64
`0x3C21`. Al usar un librarian real apareció una segunda incompatibilidad: la estática incluía
archives de imports del sistema; NSLink detectó definiciones repetidas que lld-link ocultaba por
selección perezosa.

**Corrección:** `NSC_LLD_LINK` elige el linker PE y `NSC_LLVM_LIB` elige el librarian. Se descubre
`llvm-lib` junto al linker o en `PATH`; sólo un ejecutable identificado como `lld-link` puede usar
el fallback `/lib`. `nscbuilder` entrega ambos caminos explícitamente al gate.

Una `.lib` NativeSharp contiene los objetos del productor y las dependencias estáticas
consolidadas, no las import libraries del proceso. La clausura pequeña que puede aparecer dentro
de objetos ya precompilados (`__C_specific_handler`, `memcpy`, `memset`, `memmove`, `strlen`) se
publica una sola vez al formar el PE final.

**Garantía:** las seis suites que fallaban pasan juntas tanto con NSLink + `llvm-lib` como con
lld-link. La composición B → A → ejecutable conserva el código transitivo y deja de producir
imports duplicados.

## Pruebas y resultados acumulados

La matriz relacionada con esta entrega terminó **17/17 verde**:

- carga y grafo de proyectos;
- compilación de proyectos;
- RTTI y typed catches;
- base de interoperabilidad;
- producción y consumo de bibliotecas nativas;
- ABI C;
- headers y metadata;
- hosts C, C++, C++/CLI y P/Invoke;
- compatibilidad de metadata con ownership/defaults;
- linker de Windows;
- driver.

La prueba central de bibliotecas contiene **74 comprobaciones**. No sólo comprueba existencia de
archivos: ejecuta consumidores separados de una estática, de una DLL y de una composición estática
A → B. Además ejecuta consumidores semánticos, sin `extern C` manual, para estática y DLL. Las
variantes correctas devuelven `42`; las variantes incompatibles deben fallar antes de producir un
ejecutable.

La revalidación posterior a la pasada 11 hizo pasar los **297 casos** del catálogo con lld-link:
296 dentro de la corrida principal y P/Invoke fuera del aislamiento para que el SDK pudiera leer
la configuración local de NuGet. El mismo comando del gate NSLink pasó sus 296 casos nativos con
`NSC_LLD_LINK=nslink.exe` y `NSC_LLVM_LIB=llvm-lib.exe`; P/Invoke completó el caso 297 por separado.

Ésta es evidencia local de la corrección, no una sustitución de G00. Como la pasada 11 modificó el
árbol después del baseline aportado, el gate debe volver a calcular S0/S1 y certificar el nuevo
estado antes de declarar `LANDED`.

## Pasada 12 — el fixture debe medir el contrato, no la marca del linker

La primera corrida de G00 posterior a la pasada 11 llegó a **296/297**. Los 296 casos funcionales
pasaron, incluidas las bibliotecas nativas. El único fallo fue `nsc.linker-info`: el comando produjo
correctamente:

```text
linker: C:\native-sharp\bin\nslink.exe
librarian: C:\clang-llvm\bin\llvm-lib.exe
flavor: Windows COFF/PE
target: x86_64-pc-windows-msvc
subsystem: console
```

No falló el descubrimiento ni el backend. El fixture conservaba
`PASS_REGULAR_EXPRESSION "lld-link"`, una expectativa anterior a la matriz de dos linkers. Por eso
declaró error precisamente cuando `nsc` informó que estaba usando NSLink, que era el proveedor que
esa pasada debía certificar.

La corrección cambia la aserción de integración a `flavor: Windows COFF/PE`. Ése es el contrato
estable: `--print-linker` debe terminar con éxito y anunciar un backend PE/COFF. La ruta concreta
queda visible para auditoría, pero puede ser `lld-link.exe` o `nslink.exe`; el archivador también se
informa por separado porque una biblioteca estática se produce con `llvm-lib.exe` cuando el linker
seleccionado no implementa modo `/lib`.

La lección es general: una prueba de integración no debe identificar una capacidad por el nombre
de la herramienta que primero la implementó. Debe medir la capacidad observable. Aquí esa
capacidad es “toolchain Windows PE/COFF descubierto”, no “el texto contiene lld-link”.

La revalidación posterior confirmó el cierre. En la corrida completa con NSLink, el caso 247 ya
pasó y los **296 casos nativos** terminaron verdes. El único fallo restante dentro del aislamiento
fue el host C# por no poder acceder al SDK/paquetes locales; repetido fuera de ese aislamiento,
pasó **1/1**. La composición de evidencia es, por tanto, **297/297**, sin un fallo de compilador o
linker escondido. El gate oficial todavía debe recalcular su baseline porque esta corrección mueve
el árbol.

## Frontera que todavía permanece

Los métodos estáticos con tipos base ya se consumen por nombre fuente y mangling nativo. La
frontera pendiente es ensanchar esa capacidad al contrato NativeSharp completo: tipos nominales,
layouts de valor, miembros de instancia, delegates, genéricos/constraints y dispatch entre módulos.
También falta certificar esas familias en
combinaciones transitivas y de múltiples bibliotecas sin rebajar los diagnósticos físicos ya
cerrados.

## Regla

Toda pasada que modifique el compilador debe terminar generando un `nsc.exe` nuevo que contenga
acumulativamente todos los cambios de esa pasada y de las anteriores.

Compilar bibliotecas internas, enlazar ejecutables de prueba o ejecutar pruebas contra
`nsc_driver` no sustituye la generación del compilador. Esas acciones verifican componentes, pero
el producto de una pasada de `nsc` es el ejecutable completo.

## Condición de cierre de cada pasada

Una pasada sólo puede declararse cerrada cuando existen las cuatro evidencias siguientes:

1. Los componentes modificados compilan.
2. Se genera correctamente el `nsc.exe` del árbol de trabajo correspondiente.
3. El ejecutable generado informa una identidad nueva mediante `--version`.
4. Se registra el SHA-256 del ejecutable generado.

Después se ejecutan las pruebas proporcionales al cambio. Las pruebas verdes son necesarias, pero
no convierten una pasada sin `nsc.exe` generado en una pasada terminada.

## Archivo bloqueado

Si otro proceso mantiene abierto `nsc.exe` y el enlazador no puede reemplazarlo:

- se informa que la implementación y sus pruebas pueden estar listas;
- la pasada permanece abierta;
- no se presenta como entrega completa;
- cuando el archivo queda libre, se genera `nsc.exe`, se lee su identidad y se calcula su hash;
- sólo entonces se cierra la pasada.

No se cierra una pasada usando la identidad o el hash del ejecutable anterior.

## Pasadas acumuladas

No hace falta conservar un ejecutable por cada estado intermedio. Sí es obligatorio que el
ejecutable generado al final de cada pasada contenga todos los cambios acumulados hasta ese punto.
Si varias pasadas quedaron sin materializar por un bloqueo, el siguiente `nsc.exe` debe incorporar
todas ellas y esa omisión debe quedar señalada; a partir de esta regla no se inicia una pasada nueva
dejando la anterior sin su compilador generado.

## Evidencia de adopción

La entrega acumulada actual quedó materializada como:

- versión: `nsc 0.9.0-alpha+build.20260907T021023Z.a6c15b3fca87c7ac`;
- lenguaje: `NativeSharp 0.1`, Stage 57;
- SHA-256: `E6385401C7E82FCB3896749195DD4896DA1309DF589BDCFB1E6BE8E143F55433`;
- tamaño: `3,695,616` bytes;
- generado (UTC): `2026-09-07T02:10:24Z`;
- producto: `build-tests/bin/nsc.exe`.
