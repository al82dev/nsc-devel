# 125 — NSC-LIMIT-011: constraints genéricos entre proyectos

**Fecha:** 2026-09-05
**Plan:** documento 121, §5.3
**Estado:** implementado en disco y verde en pruebas dirigidas; pendiente de matriz completa y
revalidación de Saru

---

## 1. Contrato

Una plantilla declarada en una biblioteca debe poder cerrarse con un tipo que sólo existe en el
proyecto consumidor:

```csharp
// Biblioteca
public static int Read<T>(T value) where T : class, IComponent;

// Consumidor
public class Payload : IComponent { }
Catalog.Read<Payload>(payload);
```

La cláusula y el argumento tienen autoridades distintas:

- `IComponent` se resuelve con el módulo, namespace y visibilidad de la biblioteca;
- `Payload` se resuelve con el proyecto y namespace del consumidor;
- la diagnosis de una violación apunta a la llamada del consumidor y nombra el módulo que declaró
  el contrato;
- una re-clonación propaga el origen existente y nunca lo reemplaza por el contexto de la
  biblioteca;
- dos tipos homónimos de proyectos consumidores distintos no comparten una especialización.

No se añade metadata dinámica ni un diccionario de tipos en runtime. La comprobación sigue siendo
estática y las especializaciones siguen monomorfizadas.

---

## 2. Rojos medidos antes de la corrección

### 2.1 Argumento simple del consumidor

La prueba con `Limit011Lib.Catalog.Read<Limit011Game.Payload>` no llegaba a OOP MIR. El clon de la
cláusula contenía `substitutedArgument = Payload`, pero ese nodo no llevaba origen y
`ProjectNameLookup` lo buscaba en `Limit011Lib`.

### 2.2 Consumidores homónimos

Dos proyectos distintos declaraban `Limit011SharedGame.Payload` y cerraban la misma plantilla de
la biblioteca. El primero creaba la especialización y el segundo reutilizaba ese clon porque la
clave contenía únicamente la escritura `Payload`. El segundo consumidor terminaba comprobándose
contra el tipo del primero.

### 2.3 Fuga de contexto entre sobrecargas

Dos plantillas `Choose<T>` compartían nombre, aridad genérica y cantidad de argumentos. El cuerpo
de la primera materializaba `Reader<T>` y cambiaba los campos `currentInstantiation*` al contexto
de la biblioteca. Al volver, la segunda candidata heredaba ese contexto y su `Payload` se resolvía
como si se hubiera escrito en la biblioteca.

### 2.4 Diagnosis incompleta

El rechazo ya apuntaba al `Catalog.Read<Wrong>(wrong)` del consumidor, pero el mensaje no decía
que el constraint pertenecía a `Limit011RejectLib`.

---

## 3. Cambios del compilador

### `ProjectGenericModel`

- `preserveOrAttachInstantiationOrigin` es la única operación que adjunta el proyecto y namespace
  del sitio de instanciación a un tipo sustituido.
- Se usa tanto al sustituir `T` dentro de un cuerpo como al construir
  `GenericConstraintClause::substitutedArgument`.
- Un namespace vacío se almacena explícitamente: significa el namespace global del consumidor.
- Si el tipo ya trae origen, se conserva sin re-derivarlo.
- `ensureTypeSpecialization` y `ensureMethodSpecialization` guardan y restauran los cuatro campos
  del contexto alrededor del recorrido recursivo del clon. Una especialización anidada ya no
  contamina a una candidata hermana.
- `typeSpecializationIdentity` separa la escritura pública (`Payload`) de la identidad de caché.
  La identidad incorpora el nombre estable del módulo y el namespace de origen; nunca incorpora el
  ordinal dependiente del orden del grafo. `typeArguments` y los diagnósticos siguen mostrando la
  escritura fuente.

Archivos:

- `include/nativesharp/Frontend/ProjectGenericModel.h`;
- `src/Frontend/ProjectGenericModel.cpp`.

### `ProjectNameLookup`

Cuando existe `originProjectOrdinal` sin un `originNamespace` antiguo, el namespace efectivo es el
global del proyecto de origen. Ya no cae accidentalmente al namespace del árbol de la plantilla.

Archivo:

- `src/Frontend/ProjectNameLookup.cpp`.

### `ProjectGenericConstraintModel`

Los tres mensajes de violación de una especialización agregan el módulo de la plantilla. El rango
sigue siendo `specialization.instantiationRange`, por lo que el archivo, línea y columna pertenecen
al consumidor.

Archivo:

- `src/Frontend/ProjectGenericConstraintModel.cpp`.

---

## 4. Pruebas añadidas

`tests/unit/Stage52GenericCollectionsCompilerFixesTests.cpp` cubre:

1. método genérico de biblioteca con argumento declarado sólo por el consumidor;
2. tipo genérico de biblioteca;
3. `Envelope<T> -> Reader<T>` como re-clonación anidada;
4. consumidor en namespace global;
5. dos módulos consumidores con el mismo namespace y el mismo nombre simple `Payload`;
6. grupo de sobrecargas donde la primera candidata materializa otro genérico antes de la segunda;
7. tipo inválido: una sola diagnosis, módulo de biblioteca correcto y rango exacto de la llamada
   del consumidor;
8. dos definiciones LLVM distintas para las especializaciones homónimas.

La fixture `tests/cc1/c48_constraints_cross_project/` ejecuta en Windows:

- un método genérico cross-project;
- un tipo genérico cross-project con re-clonación;
- un grupo de sobrecargas contaminante;
- resultado contractual `42` en O0 y O2.

`tests/unit/Cc1CorrectnessTests.cpp` registra la fixture. El guard de CMake reconoció 48 fixtures;
crear la carpeta sin registrarla habría detenido la configuración.

---

## 5. Evidencia dirigida

### Árbol `build-test-nslink`

```text
nsc.stage52.generic-collections-compiler-fixes                  PASS
nsc.project-name-lookup                                        PASS
nsc.project-method-body-model                                  PASS
nsc.stage44.generics-monomorphization                          PASS
nsc.stage48_2.library-metadata                                 PASS
nsc.stage52-1.generic-collections-additions                    PASS
nsc.stage59.closed-generic-member                              PASS
nsc.cc1.correctness                                            PASS
nsc.cc1.correctness.cc1-c44-constraints-genericos              PASS O0/O2
nsc.cc1.correctness.cc1-c46-sobrecargas-genericas              PASS O0/O2
nsc.cc1.correctness.cc1-c48-constraints-cross-project          PASS O0/O2
```

### Árbol `build-test-lldlinker`

```text
nsc.stage52.generic-collections-compiler-fixes                  PASS
nsc.stage48_2.library-metadata                                 PASS
nsc.cc1.correctness.cc1-c48-constraints-cross-project          PASS O0/O2
```

### Compilador

```text
build-clang, target nsc                                        PASS
```

No se ejecutó `all.bat`, no se hizo commit y no se instaló el compilador construido.

---

## 6. Lo que falta para declarar cerrado NSC-LIMIT-011

- reconstruir e instalar el `nsc.exe` exacto que medirá las pruebas que invocan el compilador
  instalado;
- ejecutar CTest completo;
- ejecutar todas las fixtures y bug triage con lld-link y NSLink;
- restaurar `where T : class, IComponent` en los tres métodos de `Entity` de Saru;
- compilar y ejecutar la suite de Saru primero en O0 y después incorporarla al contrato O2;
- comprobar que los hashes del compilador construido e instalado coinciden durante la medición.

Hasta entonces el estado correcto es **implementado y dirigido-verde**, no **certificado**.
