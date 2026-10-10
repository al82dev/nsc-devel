# Validación 165: fixture Frontend con bloque obligatorio en `for`

## SCOPE LOCK

- Objetivo: corregir exclusivamente la fixture compartida de los `40` fallos
  `Host.Frontend` cuyo `for` válido carecía de llaves.
- Productor de la fixture:
  `tests/compiler-suite/host/modules/FrontendLoweringModule.cpp`, función
  `loweringSource()`.
- Contratos afectados: `CF-FRONT-056` a `CF-FRONT-063`.
- Archivos autorizados: el productor anterior y este documento.
- Fuera de alcance: compilador, `nscbuilder3.py`, `TRACKING.md`, manifiestos,
  casos de `move`, `throw`, `some`, macros y cualquier otra causa observada.
- Validación autorizada después del commit: sólo las `40` variantes afectadas,
  mediante la ruta dirigida de Builder 3, en `Debug`; los ejecutores host
  publican optimización canónica `NA`, aunque la selección solicitada sea
  `O0`.

## Autoridad de entrada

La corrida integral conservada en
`_nscbuilder3/history/20261010-142113-a514d35c` registra en cada uno de los
`40` casos el mismo diagnóstico sobre el `Main.ns` temporal:

```text
error: expected '{' after for clause, found 'total'
```

El manifiesto declara ocho casos por contrato. `addMatrix()` asigna las
variantes `0..7`; `expectProjectStage()` reemplaza o evita la fixture para las
variantes `2`, `4` y `5`. Por ello, la intersección exacta entre los contratos
`56..63` y la fixture válida es de cinco casos por contrato: variantes `0`,
`1`, `3`, `6` y `7`, para un total de `8 * 5 = 40`.

## Inventario exacto

- `CF-FRONT-056`: `TF-FRONT-441`, `TF-FRONT-442`, `TF-FRONT-444`,
  `TF-FRONT-447`, `TF-FRONT-448`.
- `CF-FRONT-057`: `TF-FRONT-449`, `TF-FRONT-450`, `TF-FRONT-452`,
  `TF-FRONT-455`, `TF-FRONT-456`.
- `CF-FRONT-058`: `TF-FRONT-457`, `TF-FRONT-458`, `TF-FRONT-460`,
  `TF-FRONT-463`, `TF-FRONT-464`.
- `CF-FRONT-059`: `TF-FRONT-465`, `TF-FRONT-466`, `TF-FRONT-468`,
  `TF-FRONT-471`, `TF-FRONT-472`.
- `CF-FRONT-060`: `TF-FRONT-473`, `TF-FRONT-474`, `TF-FRONT-476`,
  `TF-FRONT-479`, `TF-FRONT-480`.
- `CF-FRONT-061`: `TF-FRONT-481`, `TF-FRONT-482`, `TF-FRONT-484`,
  `TF-FRONT-487`, `TF-FRONT-488`.
- `CF-FRONT-062`: `TF-FRONT-489`, `TF-FRONT-490`, `TF-FRONT-492`,
  `TF-FRONT-495`, `TF-FRONT-496`.
- `CF-FRONT-063`: `TF-FRONT-497`, `TF-FRONT-498`, `TF-FRONT-500`,
  `TF-FRONT-503`, `TF-FRONT-504`.

## Causa raíz confirmada

`loweringSource()` pretendía aportar una fuente válida a los oráculos HIR y
MIR. Sin embargo, escribía el cuerpo de `for` como una sentencia sin bloque.
La gramática real de NativeSharp exige `{` inmediatamente después de la
cláusula de `for`; el diagnóstico es correcto y la fixture contradice el
contrato de entrada válida.

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`. La falla ocurre antes de alcanzar
los modelos HIR/MIR que los ocho contratos pretenden inspeccionar y no aporta
evidencia de un defecto del compilador.

## Corrección semánticamente fiel

Se encierra la única sentencia del cuerpo entre llaves:

```text
for (int index = 0; index < count; index = index + 1)
{
    total = total + Read();
}
```

Se preservan el inicializador, la condición, el incremento, el número y orden
de llamadas a `Read()`, la acumulación en `total` y el valor retornado. No se
agrega ninguna declaración ni se cambia el alcance de una variable existente.

## Contrato de validación

Los dos archivos de esta tanda se commitean antes de ejecutar pruebas. Después
del commit se ejecutan exclusivamente los `40` identificadores inventariados,
con build `Debug` y selección `O0`; para `nsc_component_tests`, el manifiesto
normaliza esa dimensión a `NA`. Cada resultado y su log se conservarán bajo
`_nscbuilder3/history`.

## Estado previo a validación

- Auditoría causal e inventario: completos.
- Corrección de fixture: implementada.
- Commit funcional previo a pruebas: pendiente de crear.
- Reejecución exacta: pendiente.
- Cambios en compilador, Builder 3, `TRACKING.md` o manifiestos: ninguno.
