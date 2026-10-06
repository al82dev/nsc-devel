# SEM-01.06: migracion de consultas calientes de nombres

**Requisito:** `SEM-PERF-001`

**Tag:** `NSC.SEMANTICS.INDEX`

## Problema cerrado

Tres rutas resolvian un binding de nombre recorriendo el vector global: origen de escape, plegado entero y
evaluacion de constantes. El plegador usaba una consulta syntax-only porque su adaptador no conservaba el callable
que habia producido el binding.

## Cambio

- el origen de escape consulta `nameBinding(expression, callableOrdinal)`;
- `foldIntegerConstant` recibe el callable de su sitio de uso;
- `ConstantMemberResolver` conserva ese ordinal y consulta la clave completa;
- la evaluacion de constantes usa el indice para campos `const`;
- todos los call sites del plegador pasan el `methodMemberOrdinal` ya disponible.

Los recorridos por rangos de bindings usados para construir capturas de lambda no son lookups y se conservan: su
contrato es procesar cada binding producido desde un cursor estable.

## Complejidad y correccion

Las consultas migradas pasan de O(N) sobre todos los nombres del proyecto a O(1) promedio. La clave caller-aware
impide que una especializacion o materializacion que comparta sintaxis entregue el binding de otro callable. El
fallback documentado de inicializadores `const` permanece intacto cuando todavia no existe binding.

## Validacion puntual O0

Se construye y ejecuta directamente `nsc_project_method_body_model_tests`, con temporales y evidencia en `A:`. La
auditoria estatica exige cero recorridos globales inversos o completos cuyo objetivo sea buscar un nombre por sintaxis.

No se ejecuta baseline, minibaseline, CTest general, O1, O2 ni O3.

## Evidencia

- build dirigido: correcto;
- ejecucion directa: `PASS: 64 checks`;
- recorridos inversos de `nameBindings_`: cero;
- recorridos completos para lookup por sintaxis y callable: cero;
- los dos recorridos residuales procesan rangos de captura desde un cursor y no son consultas;
- la compilacion con warnings como errores no produjo hallazgos nuevos.
