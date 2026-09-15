# 316 — Análisis de duración de referencias / Reference lifetime analysis

## Resultado / Outcome

El modelo de referencias ya conserva dos hechos independientes: el origen de escape introducido
en el punto 315 y la región léxica máxima en la que ese storage está garantizado como vivo.

```nativesharp
private static void Valid(ref int caller)
{
    ref int methodAlias = ref caller;
    {
        int local = 4;
        ref int localAlias = ref local;
        ref int inherited = ref methodAlias;
    }
}
```

`localAlias` queda limitado al scope de `local`; `inherited` conserva la región del parámetro del
llamador. Crear otro alias, seleccionar un field o elemento o atravesar una llamada que devuelve
`ref` nunca ensancha la región original.

Reference analysis now retains both escape origin and maximum valid lexical region. Every alias
must live inside its source region, and address-preserving operations cannot widen that region.

## Modelo / Model

`MethodEscapeOrigin::maximumValidScopeOrdinal` guarda el scope más amplio demostrado para la
dirección. `MethodLocalSymbol::referenceLifetimeScopeOrdinal` guarda dónde nace el alias. Al ligar
un local `ref`, `scopeContains(maximum, alias)` debe cumplirse.

- local ordinario o VLA: scope de declaración del storage;
- parámetro por valor o `in`: raíz del método, con procedencia del frame;
- parámetro `ref`/`out`: raíz del método, con procedencia externa/del llamador;
- alias, field, índice o llamada `ref`: conserva la región de la dirección de origen;
- pointee de puntero crudo sin procedencia VLA: conserva el contrato unsafe existente.

El retorno `ref` continúa siendo un sink exterior al frame. Por ello sólo la procedencia externa o
del llamador puede cruzarlo; una región de local, parámetro por valor o VLA se rechaza aunque esté
oculta tras aliases o accessors.

## Cobertura / Coverage

`tests/unit/Item316ReferenceLifetimeAnalysisTests.cpp` valida:

- aliases anidados de storage del llamador y de un local;
- exposición auditable de `max-scope` y `alias-scope`;
- emisión, enlace y ejecución real en O0;
- rechazo de un local anidado retornado mediante alias;
- rechazo de un indexador `ref` sobre un struct local;
- rechazo de un elemento VLA retornado mediante alias.

Las suites de retornos e indexadores `ref`, escapes unificados, VLAs, borrows y views forman la
regresión adyacente. La entrega no agrega sintaxis, metadata, soporte de runtime ni cambios ABI.

## Mensaje de commit / Commit message

`Analiza la duración de referencias / Analyze reference lifetimes`

`Asigna regiones léxicas máximas a aliases ref, verifica que cada alias permanezca dentro de su
origen y expone la relación para auditoría. / Assign maximum lexical regions to ref aliases, verify
every alias remains within its origin, and expose the relationship for audit.`
