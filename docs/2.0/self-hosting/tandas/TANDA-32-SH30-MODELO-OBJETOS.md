# NativeSharp 2.0 self-hosting — tanda 32: modelo de objetos

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH30` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `2cb59fc25` |
| Commit de cierre | este commit (`selfhost(tanda-32): complete the semantic object model`) |

## Resultado

El analizador conoce ahora el tipo propietario mientras analiza métodos y constructores.
`this` produce un lvalue addressable del tipo actual; `base` usa el tipo base declarado en el nodo
del aggregate; un identificador no resuelto en ámbitos locales/globalles busca por último un campo
o miembro del tipo propietario. Así, `Value`, `this.Value` y `base.BaseValue` comparten identidad
semántica de miembro en vez de degradarse silenciosamente a `error`.

La superficie ya existente de declaraciones quedó fijada como contrato acumulativo:

- campos y métodos;
- constructores;
- propiedades y sus métodos `get_`/`set_`;
- indexers y sus métodos `get_Item`/`set_Item`;
- operadores (`op_Addition` en la fixture);
- herencia, `this` y `base`.

## Frontera con el backend

El backend de la rebanada escalar emite funciones libres y métodos `static`. Los métodos de
instancia y constructores ya participan en HIR/MIR y en el modelo semántico, pero no se entregan al
lowering escalar como si no necesitaran receptor: `BackendEligibleFunction` los excluye hasta que
la representación de aggregates materialice el `this` ABI. Esto evita generar código incorrecto.
La fixture ejecuta `Counter.Probe()` como método estático y devuelve 42 con ambos linkers.

## Gate SH30

La fixture `selfhost/tests/object-model/ObjectModel.ns` contiene base y derivada, campo, constructor,
propiedad, indexer, operador, `this`, `base` y método estático. El verificador exige:

- símbolos y firmas exactos;
- facts `ThisExpression`, `BaseExpression` y `MemberExpression` sin tipo `error`;
- nombres nativos estables para `Main` y `Probe`;
- ejecución 42 en O0–O3 × LLD/NSLink.

Resultado: **PASSED**, 12.97 s.

## Archivos y métodos

- `SemanticAnalyzer.AnalyzeFunction`: preserva/restaura `currentType`;
- `SemanticAnalyzer.AnalyzeExpression`: `this`, `base` y fallback de miembro implícito;
- `LlvmBackend.BackendEligibleFunction`: no confunde método de instancia con función sin receptor;
- contrato/verificador/gate SH30 y fixture `object-model`.

- [x] Identidad semántica completa de la superficie de objetos enumerada por SH30.
- [x] Ningún `this`/`base` de la fixture degrada a `error`.
- [x] Método estático de tipo ejecutado por ambos linkers.
- [ ] Baseline completo acumulado al final.
