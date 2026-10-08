# Tanda Universal 54: resultado escalar genérico cerrado en SH42

## SCOPE LOCK

| Dimensión | Alcance autorizado |
|---|---|
| Objetivo | Recuperar los símbolos `Select` y `Main` que `nsc2 --dump-symbols tests/integration/valid/cfg_features.ns` omite en O0. |
| Naturaleza | Corrección funcional localizada del backend self-host. |
| Revisión preservada | `f6b7e7930`, incluida la aprobación de `nsc.selfhost.self-hosted-generation` (181). |
| Rango auditado | `f0fad42ef^..f6b7e7930`, con revisión completa de `f0fad42ef`, `19ec3a57e` y la restauración posterior. |
| Archivo modificable | `selfhost/nsc/CodeGen/LlvmBackend.ns`. |
| Símbolos modificables | La conversión de un resultado genérico cerrado dentro de `ConvertScalar` y un helper privado usado sólo por esa frontera. |
| Protegido | Pruebas, fixtures, proyectos, verificadores, CMake, NSC Builder, clasificadores globales, ABI, condiciones, strings, bitfields y varargs. |
| Configuración | O0 con NSLink; prueba dirigida 182 y su productor SH41 cuando sea necesario regenerarlo. |
| Prohibido | Restaurar la normalización global de `f0fad42ef`/`19ec3a57e`, tocar `C:`, desplegar o ejecutar baseline/batería completa. |
| Terminación | `nsc2 --dump-symbols` vuelve a producir exactamente los símbolos que produce `nsc0`, y la prueba dirigida 182 pasa. |

## Evidencia y grafo causal

La reproducción comparativa confirmó:

- `nsc0` termina con código 0 y publica dos líneas: `Select` y `Main`;
- `nsc2` termina con código 0 pero publica cero líneas;
- `SemanticDumper.DumpSymbols` crea un `List<bool>` y comienza `DumpSymbol` con `visited[id]`;
- el indexador de `List<T>` declara un resultado `owned T`;
- al cerrar `T` como `bool`, la conversión llega al backend como `owned T -> owned bool`;
- `ConvertScalar` reconoce el origen como placeholder genérico, pero `PhysicalValueScalar` no reconoce `owned bool` porque recibe el nombre semántico completo;
- el valor en la caja genérica queda tratado como referencia en la condición y todos los símbolos parecen ya visitados.

La cadena causal es:

```text
--dump-symbols
-> SemanticDumper.DumpSymbols
-> List<bool>.this
-> resultado declarado owned T, resultado cerrado owned bool
-> ConvertScalar no desempaqueta el escalar cerrado
-> visited[id] se evalúa con la representación equivocada
-> DumpSymbol retorna antes de escribir Select y Main
```

## Cambio

`ClosedGenericScalarType` elimina iterativamente `owned`/`borrowed` únicamente del nombre físico del resultado que se está desempaquetando desde una caja genérica. `ConvertScalar` usa ese nombre sólo cuando el origen es un placeholder genérico y sólo si el destino físico es un escalar.

No se modifica el tipo semántico, el ownership, el ABI compartido ni ningún clasificador global. La complejidad del helper es `O(q)`, donde `q` es la longitud total de los calificadores de ownership; su memoria temporal es `O(q)` por las cadenas resultantes.

## Conducta preservada

- Las referencias genéricas continúan viajando como punteros.
- Los tipos no genéricos no atraviesan el nuevo helper.
- Las conversiones numéricas generales conservan su implementación anterior.
- La restauración que hizo pasar la prueba 181 permanece intacta.
- Las pruebas y sus expectativas no cambian.

## Validación dirigida

Se ejecutará después del commit, en este orden:

1. reconstrucción del productor SH41 únicamente si el artefacto `nsc2` necesita regenerarse;
2. comparación dirigida de `--dump-symbols` para `cfg_features.ns`;
3. prueba CTest 182 aislada.

No se ejecutará baseline, minibaseline ni batería completa.
