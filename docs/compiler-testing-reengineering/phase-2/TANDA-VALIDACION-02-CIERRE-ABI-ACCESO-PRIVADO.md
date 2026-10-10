# Tanda de validación 02 — Cierre ABI de acceso privado

Fecha: 2026-10-10

## Fallo demostrado

La construcción externa compiló los `337/337` objetos del target
`nsc_component_tests` y falló al enlazar. Las pruebas Sema pedían símbolos
decorados como `public`, mientras `SemanticAnalyzer.cpp` los producía como
`private`.

La causa no era ausencia de implementaciones: las definiciones existen en el
producto. Bajo el ABI MSVC, el acceso forma parte del nombre decorado. El macro
de prueba `#define private public` sólo se había aplicado a los consumidores y
no al cuerpo que produce los símbolos.

## Auditoría completa

Se cruzaron todas las referencias indefinidas de los objetos de prueba con las
definiciones privadas de los objetos de producto. El resultado completo fue:

| Productor | Incompatibilidades ABI |
|---|---:|
| `SemanticAnalyzer` | 42 |
| `DominanceBuilder` | 4 |
| `DominanceDumper` | 3 |
| `DominanceVerifier` | 0 |
| Total | 49 |

El linker sólo imprimió las primeras 20 antes de detenerse. Parser ya resolvía
la misma frontera mediante `ParserProductTestShim.cpp`. El macro de Lifetime
sólo observa almacenamiento y no llama métodos privados, por lo que no produce
una incompatibilidad de símbolos.

## Corrección

El target compila los cuerpos vigentes de Sema y Dominance mediante shims de
prueba que primero establecen la misma vista de acceso usada por los casos. No
se copia ni reimplementa lógica y no se modifica ningún archivo de producto.

```text
SemanticAnalyzer.cpp  -> SemaProductTestShim.cpp
DominanceBuilder.cpp  -> DominanceBuilderProductTestShim.cpp
DominanceDumper.cpp   -> DominanceDumperProductTestShim.cpp
```

## Estado exacto

```text
Objetos de la corrida externa: 337/337 compilados
Incompatibilidades ABI inventariadas: 49/49 tratadas
Código de producto modificado: no
Enlace posterior: PENDIENTE DE LA CORRIDA DEL USUARIO
Ejecución funcional: PENDIENTE DE LA CORRIDA DEL USUARIO
```
