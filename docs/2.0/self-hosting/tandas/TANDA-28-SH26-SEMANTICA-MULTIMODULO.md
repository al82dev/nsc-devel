# NativeSharp 2.0 self-hosting — tanda 28: semántica multimódulo

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH26` |
| Rama | `development-claude` |
| Fecha | 2026-09-20 |
| Depende de | tanda 27 (SH25) cerrada |
| Commit de entrada | `b8e5eedb5` |
| Commit de cierre | este commit (`selfhost(tanda-28): resolve symbols across the compilation set`) |

> El código compila y SH26 dirigido pasa O0–O3. El baseline completo se ejecutará acumulado al
> terminar las tandas restantes.

## 2. Objetivo, alcance y exclusiones

SH26 da al conjunto de compilación **una sola identidad de símbolos**: un nombre declarado en un
archivo resuelve desde otro, los tipos duplicados entre unidades se diagnostican, y los callables
con el mismo nombre forman conjuntos de sobrecarga.

### 2.1 El alcance, que es el del roadmap y no una comodidad

SH25 fijó el conjunto de compilación como **las unidades del proyecto raíz**. La fila de SH26 dice
«referencias, **metadata externa**»: una `LibraryReference` se consume mediante registros públicos
`symbol|...` anexados al contrato textual `NSMETA/2`, no reparseando sus fuentes. La fixture
positiva prueba esa ruta con `ExternalFixture.ExternalValue`, que no existe en ningún `.ns`.
Reconstruir Core, Collections, IO y LLVM.C desde fuente es **SH40**.

De ahí los dos números, distintos a propósito:

* **el grafo** tiene 92 unidades — todas las de los cinco proyectos (SH25);
* **el conjunto de compilación** tiene 23 — las del proyecto raíz;
* los cuatro `ProjectReference` bootstrap todavía aportan identidad de proyecto, no sidecars,
  porque esas bibliotecas se reconstruyen desde fuente en SH40; una referencia binaria que sí
  declara `Metadata="...nsmeta"` aporta sus símbolos desde ahora.

### 2.2 Lo que esto resuelve del fallo observado en la 27

La primera corrida del gate de SH25 pasó el front-end por las 92 unidades y las de las bibliotecas
fallaron. **Medido:** `SemanticAnalyzer` no llama a `AddError()` ni una vez en sus 41 KB, y un
identificador que no resuelve no es un error — línea 282: el símbolo queda en `-1`, el tipo pasa a
`error` y el análisis sigue. Por tanto `semantic.ErrorCount()` es siempre 0 y aquellos errores
venían del **lexer o del parser**, no de la resolución de nombres.

Esta tanda no arregla eso: lo **deja fuera de alcance por la vía correcta**, que es la que el
roadmap ya tenía escrita. Si el front-end de `nsc-next` acepta o no las fuentes de biblioteca es
una pregunta localizada y diferida a SH40, no un hueco del plan.

### 2.3 Fuera de alcance

Unificación de layout y tamaños entre módulos — un tipo nominal importado se interna con
`SetLayout(0, 1)` hasta que su declaración se ve. Eso es **SH29** («verificación ABI de tamaños y
alineación»). Resolución de sobrecarga por tipos de argumento entre módulos, e identidad de
instancias genéricas entre proyectos, son **SH34**.

## 3. Diseño

### 3.1 Por qué la identidad de tipos viaja entre modelos

`TypeTable.Find` compara `Display()`: **la identidad de tipos es nominal por texto**. Por eso un
export puede llevar el texto del tipo y que el modelo receptor lo interne y obtenga el mismo
significado, aunque los dos modelos no compartan ni un solo id de tipo.

Para un callable ese texto es su firma — `(int, int) -> int`, producida por `TypeTable.Function` —
y eso es lo que permite distinguir sobrecargas sin reconstruir listas de parámetros.

### 3.2 Dos reglas de identidad, y no son la misma

* un **tipo** no puede repetir nombre cualificado en todo el conjunto;
* un **callable** sí puede: eso es una sobrecarga. Lo que no puede repetirse es el par
  (nombre cualificado, firma) — el tipo de retorno no es parte de la identidad de sobrecarga.

### 3.3 Dos pasadas, y por qué no una

* **Pasada A**: sobre cada unidad, lexer + parser + `DeclareOnly()`. Sólo declaraciones: el índice
  no necesita más, y analizar cuerpos aquí duplicaría el coste de cada build.
* **Pasada B**: analizador nuevo por unidad, presembrado con los globales de **las otras** unidades
  mediante `DeclareExternal`, y `Analyze()`.

**`SemanticAnalyzer` no cambia de lógica.** `Lookup` ya camina hasta el ámbito 0; lo que cambia es
que el ámbito 0 ahora contiene el resto del programa. El comportamiento de un archivo suelto es
idéntico al de antes, que es lo que protege el contrato de SH06.

### 3.4 La trampa que el guardia evita

Un símbolo inyectado por `DeclareExternal` lleva `declaration = -1`. El segundo bucle de
`Analyze()` recorre todos los símbolos y llama a `AnalyzeFunction` o lee
`tree.Node(symbol.Declaration())`. Sin el guardia `Declaration() < 0`, **cada función importada
provocaría `tree.Node(-1)`**. El guardia está y la razón queda escrita junto a él.

## 4. Archivos

| Archivo | Acción |
|---|---|
| `selfhost/nsc/Sema/ModuleSet.ns` | crear |
| `selfhost/nsc/Sema/SemanticAnalyzer.ns` | modificar — `DeclareOnly`, `Model`, `DeclareExternal`, guardia |
| `selfhost/nsc/Driver/Program.ns` | modificar — `--dump-module-set`, `--dump-module-exports` |
| `selfhost/nsc/Projects/ProjectSystem.ns` | modificar — descubrir `LibraryReference Metadata` |
| `selfhost/contracts/module-set.contract.json` | crear |
| `selfhost/bootstrap/verify_module_set.py` | crear |
| `cmake/VerifyNativeSharpModuleSet.cmake` | crear |
| `CMakeLists.txt` | modificar |
| `selfhost/tests/module-set/positive/*` | crear — resolución cruzada y overload válido |
| `selfhost/tests/module-set/negative/*` | crear — firma duplicada rechazada |

`ModuleSet.Build` recibe el propietario raíz **como parámetro**, no lo lee del grafo, para no
depender de un accesor que se esté añadiendo en la tanda 27 en paralelo. El llamador ya tiene el
`ProjectModel` y su `Name()` es ese valor.

## 5. Contrato

`nativesharp-selfhost-module-set-contract` v1: 23 unidades compiladas, los cuatro proyectos
externos en orden, cero sidecars en el proyecto del compilador, **cero problemas de identidad**,
**cero errores de front-end** y al menos una resolución observable hacia otra unidad. La fixture
positiva exige un sidecar, indexa su símbolo y lo resuelve desde código fuente. Las dos superficies —`--dump-module-set` y
`--dump-module-exports`— se comparan byte a byte entre O0–O3.

`expected_units: 23` = las 22 de la tanda 27 más `Sema/ModuleSet.ns`.

`expected_front_end_errors: 0` incluye los dos archivos que esta tanda añade o cambia. Si mi propio
código no parsea, el gate lo dice.

## 6. Validación

La primera compilación falló porque `set` y `export` se usaron como identificadores, aunque son
palabras reservadas de NativeSharp. Se renombraron a `moduleSet` e `item`; los diagnósticos
restantes eran cascada.

El gate original pasó después de esa corrección, pero sólo probaba que el índice y sus volcados
eran deterministas. Habría seguido verde si se eliminaba toda la inyección de símbolos. El gate se
endureció con tres observaciones obligatorias:

- el compilador real registra **1113 exports** y **2845 resoluciones
  cruzadas** entre sus 23 unidades;
- la fixture positiva resuelve `FromA` desde otra unidad y acepta dos overloads
  `ModuleSetFixture.Select` con firmas diferentes; además consume un sidecar `NSMETA/2` y resuelve
  `ExternalFixture.ExternalValue` sin fuente del proveedor;
- la fixture negativa informa exactamente la firma duplicada contratada.

Resultado final dirigido: **PASSED** en 14.03 s; O0, O1, O2 y O3 producen superficies idénticas.
El baseline completo LLD/NSLink queda diferido a la barrida acumulada final.

## 7. Riesgos y deuda

- La presiembra inyecta **todos** los globales de las otras unidades en el ámbito 0, sin filtrar por
  `using`. Es más permisivo que C#: un nombre de otro espacio de nombres resuelve sin importarlo.
  Endurecerlo exige que el analizador consuma las directivas `using`, y hoy no las mira.
- `OverloadCount` existe y la identidad de overloads se verifica; elegir por tipos de argumento es
  SH34.
- El índice es lineal en el número de exports; con 23 unidades no se nota.

- [x] Compila.
- [x] `nsc.selfhost.module-set` pasa O0–O3.
- [x] Resolución cruzada e identidad positiva/negativa de overloads observables.
- [ ] Baseline completo acumulado con LLD y NSLink al terminar las tandas.
- [ ] Promoción autoalojada completa; sigue bloqueada por SH27–SH41.
