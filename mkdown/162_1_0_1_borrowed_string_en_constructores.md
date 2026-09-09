# 162 — NativeSharp 1.0.1: `borrowed string` en constructores / Borrowed strings in constructors

## 1. Caso real

Los métodos ordinarios ya podían prestar un `owned string` fresco durante la expresión completa,
pero la resolución de constructores usaba una ruta de ownership distinta. Estas formas fallaban:

```nativesharp
raii Utf8Copy a = new Utf8Copy("literal");
raii Utf8Copy b = new Utf8Copy("left" + "right");

public ChainedCopy() : this("i" + "j")
{
}
```

El diagnóstico afirmaba que un valor movido no podía prestarse. El mismo valor temporal sí era
válido al llamar a un método, de modo que el contrato de expresión completa dependía
incorrectamente de si el destino se escribía como método o constructor.

## 2. Regla nativa

`borrowed string` no toma propiedad ni prolonga la vida de su origen:

- un `string-literal` puede prestarse directamente porque sus bytes duran todo el proceso;
- un parámetro ya borrowed puede cruzar el constructor sin transferencia;
- un resultado `owned string` fresco puede prestarse hasta terminar la expresión completa;
- el compilador conserva ese temporal en un owner oculto y lo destruye exactamente una vez al
  terminar la construcción, también por la ruta excepcional;
- el constructor no puede guardar el parámetro borrowed en un campo que viva más tiempo.

No se debilitó el análisis de escapes y no se convirtió `borrowed` en ownership compartido.

## 3. Archivos, funciones y estructuras modificados

### Modelo de construcción

- `include/nativesharp/Frontend/ProjectConstructionModel.h`
  - `ConstructorCandidateBinding::fullExpressionBorrowArgumentOrdinals`;
  - `ConstructorInitializerBinding::selectedFullExpressionBorrowArgumentOrdinals`;
  - `ObjectConstructionBinding::selectedFullExpressionBorrowArgumentOrdinals`.
- `src/Frontend/ProjectConstructionModel.cpp`
  - helpers `isStringLiteralBorrow` e `isFreshOwnedStringBorrow`;
  - `bindConstructorInitializer` acepta y registra temporales prestados en `this(...)`/`base(...)`;
  - `bindConstruction` aplica la misma regla a `new T(...)`;
  - un préstamo fresco no se registra como transferencia de ownership.

### Análisis de llamadas

- `src/Frontend/ProjectMethodBodyModel.cpp`
  - `ProjectMethodBodyModel::analyzeCall` reconoce que un literal de string tiene duración de
    proceso cuando el parámetro exige explícitamente `borrowed string`;
  - conserva los rechazos ordinarios para valores sin owner comprobable.

### HIR y cleanup

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
  - declara `wrapFreshOwnedTemporaryBorrow`.
- `src/Frontend/ProjectOopHirModel.cpp`
  - `ProjectOopHirModel::build` envuelve argumentos temporales de inicializadores de constructor;
  - `lowerExpressionImpl` hace lo mismo para construcciones de objetos;
  - `wrapCallArgumentBorrow` delega la ruta común;
  - `wrapFreshOwnedTemporaryBorrow` crea `BorrowValue`, el owner sintético y
    `OwnedTemporaryBorrow`;
  - `wrapFullExpressionCleanup` rodea la llamada o construcción sólo cuando existe un temporal que
    limpiar.

### Contrato y pruebas

- `tests/cc1/c49_temporary_string_borrow/Main.ns`
  - cubre literal, parámetro borrowed, concatenación owned fresca, `new T(...)` y `this(...)`;
  - devuelve 42 en O0/O2 con ambos enlazadores.
- `tests/unit/Stage50_7BorrowEscapeAnalysisTests.cpp`
  - agrega `ConstructorFieldStore`, que debe seguir rechazando guardar el préstamo en un campo.
- `spec/NativeSharp_Language_Specification_1_0.md`
  - extiende `NS-OWN-003` a métodos y constructores.
- `spec/NativeSharp_1_0_conformance.map`
  - enlaza `NS-OWN-003` con Construction, OopHir, Stage50_7, Stage58 y c49.

## 4. Motivo de la implementación

La resolución de candidatos necesita saber qué argumento requiere un owner temporal, pero no debe
crear HIR. Por eso registra ordinales en el binding elegido. HIR consume esos ordinales después de
aplicar conversiones y construye el mismo patrón de vida que ya usaban las llamadas ordinarias.

Centralizar la creación del owner en `wrapFreshOwnedTemporaryBorrow` evita dos representaciones
distintas para el mismo préstamo. Mantener separados “préstamo temporal” y “transferencia” impide
que el constructor consuma el valor y además intente destruirlo al final de la expresión.

## 5. Verificación

Pruebas enfocadas previas al baseline:

- `Stage50_7BorrowEscapeAnalysisTests`: 38/38 con LLD y 38/38 con NSLink;
- fixture c49 seleccionada: 6/6 con LLD y 6/6 con NSLink;
- `Stage60LanguageContractTests`: 1014 comprobaciones con cada enlazador;
- compilador instalado `5b2e13e1809ba38e`: el reproductor exacto devolvió 42 en O2.

Baseline completo definitivo:

```text
run_id:     20260908-210330-3a9390d3
estado:     LANDED, certified=true
duración:   682.304 s
CTest:      306/306 LLD, 306/306 NSLink
fixtures:   148/148 LLD, 148/148 NSLink
triage:     18/18 LLD, 18/18 NSLink
fallos:     0
```

La corrida roja anterior, `20260908-204545-ec8fea99`, también mostró que c49 pasaba; sus fallos
pertenecían exclusivamente a la regresión de `ulong` documentada en el acta 161.

## 6. Commit de referencia / Reference commit

### Título en español

`Permite préstamos temporales de string en constructores`

### English title

`Allow temporary string borrows in constructors`

### Descripción en español

Aplica a `new`, `this` y `base` la misma regla de préstamo de expresión completa usada por las
llamadas ordinarias. Mantiene vivos los strings owned frescos mediante un owner oculto, permite
literales con duración de proceso y conserva el rechazo de escapes desde parámetros borrowed.

### English description

Apply the same full-expression borrow rule used by ordinary calls to `new`, `this`, and `base`.
Keep fresh owned strings alive through a hidden owner, allow process-lifetime literals, and retain
escape rejection for borrowed parameters.
