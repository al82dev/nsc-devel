# Handover a NSLink — compatibilidad con librerías precompiladas NativeSharp 1.0.1

## 1. Propósito

Este documento entrega al worker de NSLink el contrato binario que empezó a
ejercitarse al habilitar consumo real de librerías NativeSharp precompiladas.
Separa deliberadamente:

1. cambios internos de NSC que NSLink no debe conocer;
2. cambios que aumentan la superficie de objetos, archives y DLL;
3. formas COFF/PE que NSLink sí debe aceptar y resolver;
4. el defecto confirmado que bloquea hoy la generación de la DLL;
5. una matriz de auditoría para encontrar de una vez las incompatibilidades
   vecinas, sin esperar a que el binding revele una por compilación.

Este no es un pedido para acoplar NSLink a NSMETA ni al frontend NativeSharp.
El linker debe continuar siendo un consumidor general de COFF AMD64 y productor
de PE32+.

## 2. Estado entregado

### Compilador instalado

```text
Ruta:    C:\native-sharp\bin\nsc.exe
Versión: nsc 0.9.0+build.20260911T080123Z.82d51cd3f90b2232
Build:   82d51cd3f90b2232
SHA-256: AA5BFCAEF29B288FEC4DE2470D335EDFB1FF4D90552D809C1B2768FAEC8268EE
```

### Linker observado por el binding

```text
Ruta:        C:\native-sharp\bin\nslink.exe
Fecha UTC:   2026-09-10 18:59:49
SHA-256:     64175377969EA55CCFA40B80982B2FB799EA6220E4ABC4B97F4633E9BB08D4D4
```

### Bloqueo actual

```text
NSL1025: COMDAT section has no section definition symbol:
section '.xdata' has no symbol to identify it
```

La biblioteca estática, el header y NSMETA se generan correctamente. El fallo
ocurre al enlazar el `program.obj` del producto DLL en `-O0`. El texto
`lld-link failed` emitido por NSC es una etiqueta histórica genérica; el
ejecutable seleccionado y ejecutado por el script es el `nslink.exe` anterior.

## 3. Conclusión ejecutiva

No todos los cambios de librerías requieren trabajo en NSLink. Los cambios de
tipos, accesibilidad y metadata terminan antes de generar el objeto. Sin
embargo, al superar esos bloqueos NSC entrega ahora objetos mucho más completos:

- especializaciones genéricas compartidas por productor y consumidor;
- funciones y datos `weak_odr`;
- COMDAT por símbolo;
- constructores, destrucción, descriptors, vtables e interfaces;
- exports DLL aunque no exista uso interno;
- `.pdata` y `.xdata` asociativas para funciones COMDAT;
- archives con extracción transitiva y definiciones alternativas.

El `NSL1025` es el primer defecto confirmado de NSLink en esta nueva superficie.
Conviene corregirlo primero y después ejecutar el barrido de las secciones 7–9
para evitar una secuencia nocturna de un defecto por compilación.

## 4. Historia reciente de NSC y efecto sobre NSLink

| Commit NSC | Cambio | Efecto binario | Trabajo esperado en NSLink |
|---|---|---|---|
| `551bf7e3` | Conserva símbolos físicos precompilados | Los consumidores referencian exactamente los nombres publicados por el productor | Resolver nombres COFF sin reinterpretar mangling |
| `c8980c32` | Deduplica genéricos con COMDAT | Introdujo `weak_odr` y COMDAT explícito para funciones y datos genéricos | Aceptar selección COMDAT y símbolos débiles LLVM |
| `8749fcd9` | Separa COMDAT por símbolo genérico | Productor y consumidor pueden aportar subconjuntos distintos de una especialización | Resolver cada símbolo independientemente; no descartar toda la especialización |
| `7b45956b` | Empaqueta sólo runtimes alcanzados | El conjunto de DLL de entrada puede ser menor y depender de uso real | No asumir una DLL por cada metadata o archive mencionado durante compilación |
| `eeb9597e` | Deduplica runtimes entre rutas | Una misma DLL puede llegar nombrada por rutas distintas | Mantener identidad coherente de inputs y no duplicar imports equivalentes |
| `1b866813` | Usa el símbolo físico como líder COMDAT | El símbolo real es la clave `Any`; no existe el antiguo byte global `__nsc_generic_odr_*` | Aceptar `.xdata/.pdata` asociativas aunque no tengan clave ordinaria propia |
| `dc9d6b65` | Delega destrucción de bases precompiladas | La destrucción local llama indirectamente la entrada del descriptor de la base | Conservar descriptor/accessor/destruction entry y sus relocations durante GC |

Los siguientes commits fueron esenciales para las librerías, pero no introducen
una construcción nueva que NSLink deba interpretar:

| Commits NSC | Responsabilidad |
|---|---|
| `05158188`, `bb7295f6` | Clausura y superficie pública de especializaciones genéricas en NSMETA/HIR |
| `790917c7` | Validación temprana de contratos precompilados |
| `e2c6457b`, `7e9028dd` | Reconstrucción de interfaces y nombres calificados |
| `e72823d2`, `42bf6f5a`, `7411b66b` | Accesibilidad efectiva, interfaces y arreglos en firmas públicas |
| `4fc8329f`, `7cac14b0`, `a2c44ad1` | Identidad nominal externa, eventos y conversiones durante reconstrucción NSMETA |

Esos cambios pueden incrementar el número de funciones, datos y relocations del
objeto, pero NSLink no debe parsear NSMETA ni reproducir sus reglas de tipos.

## 5. P0 confirmado — COMDAT asociativo sin símbolo clave propio

### Ubicación

Árbol NSLink, sólo como referencia para el worker:

```text
src/link/LinkGraph.cpp
```

En `LinkGraph::addObject()`, después de leer el auxiliar
`SectionDefinition`, el código hace aproximadamente lo siguiente:

```cpp
section.setComdat(auxiliary.selection(), auxiliary.number());

for (std::size_t next = index + 1; next < coffSymbols.size(); ++next)
{
    if (coffSymbols[next].sectionNumber() == symbol.sectionNumber())
    {
        section.setKeySymbol(...);
        break;
    }
}

if (section.keySymbol() == kNoIndex)
{
    // NSL1025
}
```

La exigencia se aplica actualmente a todas las selecciones COMDAT. Ésa es la
causa inmediata del error.

### Regla COFF necesaria

Una sección primaria con selección `ANY`, `LARGEST`, `EXACT_MATCH`,
`SAME_SIZE` o `NODUPLICATES` necesita una clave que identifique el grupo.

Una sección con:

```text
IMAGE_COMDAT_SELECT_ASSOCIATIVE = 5
```

no posee un grupo independiente. Su registro auxiliar `SectionDefinition`
contiene en `Number` el número uno-basado de la sección primaria. Su vida y
selección dependen de esa sección. LLVM puede emitir `.xdata` o `.pdata` sin un
símbolo ordinario posterior definido dentro de la propia sección asociativa.

### Corrección requerida en NSLink

Después de `setComdat()`:

1. Si la selección es `Associative`, no buscar ni exigir `keySymbol`.
2. Conservar `auxiliary.number()` como `associatedSection`.
3. Dejar que `ComdatResolver` valide la existencia de la sección primaria.
4. Continuar exigiendo `keySymbol` para COMDAT primarios.
5. Conservar `NSL1025` para un primario realmente anónimo.
6. Conservar `NSL1024` cuando `associatedSection` no exista.

`src/link/ComdatResolver.cpp` ya está dividido correctamente en dos pasadas:
omite las asociativas durante la selección primaria y luego consulta
`associatedSection()`. El rechazo ocurre antes de llegar a esa lógica.

### Regresiones mínimas obligatorias

1. `.text` primaria `ANY` con símbolo clave: aceptada.
2. `.xdata` `ASSOCIATIVE` a esa `.text`, sin símbolo ordinario propio:
   aceptada.
3. `.pdata` `ASSOCIATIVE` equivalente: aceptada.
4. Primaria sin símbolo clave: `NSL1025`.
5. Asociativa cuyo `Number` no existe: `NSL1024`.
6. Cuando pierde la primaria, también se descartan sus `.xdata` y `.pdata`.
7. Cuando gana la primaria, ambas auxiliares permanecen vivas.
8. El `program.obj` real que motivó el reporte enlaza como DLL.

## 6. Clave primaria: endurecimiento recomendado

La búsqueda actual toma «el siguiente símbolo definido en la misma sección».
Eso funciona para parte del corpus, pero es una aproximación dependiente del
orden. Durante el barrido conviene comprobar que la clave elegida sea realmente
un símbolo apto para identificar el COMDAT primario, no otro símbolo de sección,
un label local o un registro que casualmente aparezca después.

El corpus debe cubrir al menos:

- símbolo externo de función;
- símbolo externo de dato;
- símbolos estáticos y labels adicionales en la misma sección;
- registro auxiliar de función;
- weak external con fallback;
- nombre largo almacenado en la string table.

No se debe inventar una clave para un primario malformado. Ese caso debe seguir
fallando con `NSL1025`.

## 7. Barrido P0 después de desbloquear `NSL1025`

### 7.1 Resolución de COMDAT por símbolo

NSC ya no agrupa toda una especialización. Cada entidad tiene grupo propio. El
linker debe permitir que productor y consumidor materialicen subconjuntos
distintos:

```text
productor:  constructor + descriptor + método A
consumidor: constructor + descriptor + método B
```

La selección de `constructor` no debe descartar `método A` ni `método B`.

Comprobar:

- `ANY` elige una definición de forma determinista;
- cada símbolo físico usa su propia competencia;
- las relocations hacia el símbolo perdedor se redirigen al ganador;
- una relocation nunca queda apuntando a una sección descartada;
- las asociativas siguen exactamente a su primaria ganadora o perdedora.

### 7.2 Weak externals de LLVM

LLVM/COFF puede publicar un símbolo débil y un fallback mediante un auxiliar
`WeakExternal`. También pueden aparecer nombres de import library con forma:

```text
.weak.<simbolo>.default.<simbolo>
```

Comprobar:

- resolución del símbolo preferido cuando existe;
- uso del fallback cuando no existe definición fuerte;
- preferencia de una definición fuerte sobre la débil;
- preservación del nombre exportado, no del alias interno;
- relocations remapeadas al símbolo finalmente seleccionado.

### 7.3 Extracción transitiva de archives

Las librerías estáticas precompiladas contienen cuerpos, descriptors y
especializaciones que pueden descubrir nuevas referencias al extraerse.
NSLink debe iterar hasta punto fijo:

1. comenzar con símbolos no resueltos del programa;
2. extraer el miembro del archive que define uno;
3. agregar las nuevas referencias de ese miembro;
4. repetir hasta clausura;
5. aplicar COMDAT sobre todos los miembros efectivamente incorporados.

Casos obligatorios:

- el constructor extrae un miembro que referencia descriptor y destruction
  entry ubicados en otros miembros;
- dos archives ofrecen la misma especialización COMDAT;
- sólo uno ofrece un método adicional todavía requerido;
- el orden de los archives no cambia el resultado válido;
- un archive no extraído no introduce falsos duplicados.

### 7.4 Garbage collection y raíces

Son raíces de enlace:

- entry point;
- exports DLL solicitados;
- tablas de importación necesarias;
- símbolos alcanzados desde relocations vivas.

Las aristas deben incluir:

- descriptor → vtable;
- descriptor → interface map;
- descriptor → destruction entry;
- vtable/interface table → métodos;
- función → `.pdata`/`.xdata` asociativas;
- accessor → almacenamiento o descriptor;
- destruction entry → destructores, campos y deallocator.

Un export genérico debe sobrevivir aunque el productor no lo llame
internamente.

## 8. Barrido P1 — DLL, import libraries y excepciones

### 8.1 Exportación DLL

Comprobar juntos:

- función normal;
- función `weak_odr`/COMDAT;
- constructor;
- accessor de descriptor;
- accessor de campo estático;
- thunk de interfaz o COM;
- función que sólo existe porque forma parte de la API publicada.

La tabla de exports debe contener el nombre solicitado después de resolver
weak/COMDAT, y la import library debe apuntar a ese nombre público.

### 8.2 Imports de función y datos

PE usa distintas indirections para código y datos. Mantener casos separados
para:

- llamada a función importada;
- lectura de dato importado;
- llamada a accessor que devuelve descriptor/dato real;
- thunk respaldado por `.lib` nativa;
- imports `extern C from "dll"`.

### 8.3 Win64 unwind

Para funciones ordinarias, genéricas, destructores, callbacks y wrappers C:

- `.pdata` debe conservar registros `RUNTIME_FUNCTION` completos;
- `.xdata` debe conservar `UNWIND_INFO` y handlers;
- las relocations `ADDR32NB` deben resolverse después del layout;
- el exception directory de PE debe cubrir los registros vivos;
- una sección asociativa descartada no debe contaminar la tabla final;
- una sección viva no debe desaparecer por GC.

## 9. Matriz recomendada de artefactos

El barrido debe usar objetos LLVM reales, no sólo objetos sintéticos. Un corpus
pequeño puede cubrir la combinación completa:

| Caso | Producto | Rasgo principal | Resultado requerido |
|---|---|---|---|
| A | EXE | función genérica COMDAT con `.pdata/.xdata` | enlaza y ejecuta |
| B | DLL | export genérico sin llamadas internas | export e import lib presentes |
| C | EXE + static `.lib` | productor/consumidor repiten una especialización | una definición por símbolo |
| D | dos static `.lib` | subconjuntos diferentes de la misma especialización | ningún símbolo requerido descartado |
| E | DLL base + DLL derivada | descriptor y destrucción heredada | accessor/import y unwind válidos |
| F | EXE | excepción o cleanup dentro de función genérica | stack-unwind válido |
| G | DLL | thunk COM más callbacks/delegates | exports, relocations y unwind válidos |
| H | EXE/DLL O0 y O2 | misma superficie con distinta optimización | ambos enlazan; exports equivalentes |

Para cada caso comparar NSLink contra `lld-link` como oráculo externo:

- éxito o diagnóstico;
- lista de símbolos no resueltos;
- tabla de exports;
- import directory;
- exception directory;
- secciones `.pdata`, `.xdata`, `.reloc` y `.debug$*` cuando correspondan;
- ausencia de header CLR;
- código de salida cuando el caso sea ejecutable.

No se exige que los bytes completos del PE sean idénticos: timestamps, orden y
layout pueden diferir. Sí deben coincidir la semántica, ABI y superficie
observable.

## 10. Criterios de aceptación acumulativos

NSLink queda listo para esta fase cuando:

1. El `program.obj` actual deja de producir `NSL1025`.
2. Las asociativas sin clave propia se enlazan mediante `associatedSection`.
3. Los casos rojos `NSL1024` y `NSL1025` continúan distinguiéndose.
4. Productor y consumidor pueden repetir símbolos genéricos sin duplicados.
5. Ninguna relocation viva apunta a una sección descartada.
6. La extracción de archives alcanza punto fijo.
7. Los exports `weak_odr` sobreviven aunque no tengan uso interno.
8. `.pdata/.xdata` siguen a la función correcta durante selección y GC.
9. Static library, DLL e import library funcionan en O0 y O2.
10. El resultado relevante coincide semánticamente con `lld-link`.
11. El `nslink.exe` corregido se instala realmente en
    `C:\native-sharp\bin\nslink.exe` y su versión/hash quedan registrados.

## 11. Lo que NSLink no debe implementar

No corresponde al linker:

- leer NSMETA;
- decidir accesibilidad NativeSharp;
- reconstruir interfaces o genéricos;
- conocer `EventHandler<T>`, `Mat4` o nombres del framework;
- inventar símbolos físicos ausentes;
- reparar un ABI incompatible entre productor y consumidor;
- materializar destructores o cuerpos HIR;
- deducir dependencias desde rutas de proyectos.

NSC entrega el contrato ya convertido en símbolos, secciones y relocations.
NSLink debe implementar correctamente las reglas COFF/PE aplicables a ese
contrato.

## 12. Orden de trabajo recomendado

1. Corregir exclusivamente el rechazo de asociativas en
   `LinkGraph::addObject()`.
2. Agregar los cinco casos estructurales de `NSL1024/NSL1025`.
3. Enlazar el `program.obj` real y confirmar cuál es el siguiente resultado.
4. Ejecutar en una sola ronda la matriz COMDAT + weak + archive + GC.
5. Corregir por categoría, no por nombre de binding.
6. Ejecutar la matriz DLL/import/unwind.
7. Instalar `nslink.exe` y registrar versión, fecha y SHA-256.
8. Sólo entonces regresar el producto al worker del binding.

## 13. Commit de referencia para el P0

### Español

**Título:** Acepta secciones COMDAT asociativas sin clave propia

**Descripción:** Distingue COMDAT primarios de secciones `ASSOCIATIVE` al
construir el grafo. Conserva la exigencia de símbolo clave y `NSL1025` para
grupos primarios, pero resuelve `.xdata` y `.pdata` mediante el número de
sección asociada y mantiene `NSL1024` para referencias inexistentes.

### English

**Title:** Accept associative COMDAT sections without their own key

**Description:** Distinguish primary COMDAT groups from `ASSOCIATIVE` sections
while building the graph. Keep the key-symbol requirement and `NSL1025` for
primary groups, resolve `.xdata` and `.pdata` through their associated section
number, and retain `NSL1024` for missing associations.

## 14. Commit de referencia para el barrido

### Español

**Título:** Certifica objetos LLVM de librerías precompiladas

**Descripción:** Agrega una matriz COFF/PE con COMDAT por símbolo, weak
externals, extracción transitiva de archives, garbage collection, exports DLL y
unwind asociativo. Contrasta productos estáticos y dinámicos O0/O2 contra
`lld-link` y conserva casos rojos estructurales.

### English

**Title:** Certify LLVM objects from precompiled libraries

**Description:** Add a COFF/PE matrix covering per-symbol COMDAT, weak
externals, transitive archive extraction, garbage collection, DLL exports, and
associative unwind data. Compare O0/O2 static and dynamic products against
`lld-link` while preserving structural negative cases.
