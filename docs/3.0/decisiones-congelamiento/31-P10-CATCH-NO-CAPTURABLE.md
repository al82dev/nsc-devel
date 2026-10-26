# Congelamiento 3.0 P10 — la variable del catch no se captura en un lambda

Fecha: 2026-10-26. Revisión de partida: `c8b8f42bc`.

## Problema

Hallazgo del doc 30. La variable del catch es el parámetro que recibe la excepción atrapada, así que P10 la
cubre: no puede ir a un closure. DEC-10 rechazaba guardarla en campos, statics, elementos, locales exteriores y
`return`, pero un lambda que la capturaba compilaba:

```
catch (Boom problem) { Saved = () => problem.Code; }   // Saved es un static
```

El catch destruye la excepción al terminar el handler y el delegate guardado la lee después (sonda
`p11CatchLambda`: compilaba y corría, 1 destrucción, con el delegate vivo apuntando a la excepción liberada).

## SCOPE LOCK

- **Cambia.** `LambdaDelegateBinder.cpp`: capturar la variable del catch, o un local del handler que la copia, es
  error. Para sobrevivir al handler se saca antes con `move(e)` a un dueño. Las dos comprobaciones de excepciones
  de las capturas (esta y la de P10) pasan a `validateExceptionLocalCapture` y
  `validateExceptionParameterCapture`, así las funciones de validación de capturas no crecen.
- **Idéntico.** Las capturas de cualquier otro local o parámetro; un lambda del handler que no captura la
  excepción.

## Validación

nsc Release recompilado. Sondas `d17`:

| Sonda | Ahora |
|---|---|
| p11CatchLambda | rechazo «the caught exception in 'problem' belongs to its catch until the handler ends and cannot be captured by a lambda…» |
| p12LambdaInHandler (lambda sin captura dentro del handler) | 121 |
| p6Lambda (parámetro de P10) | rechazo, mismo mensaje que en el doc 30 |

Regresión `d10`, `d17` y proyectos con `catch`: igual que en el doc 30. Casos `nsc_component_tests` Debug y Release:
96 PASS de referencias, 30 PASS de copia. Corpus de `await`: igual que `d04`.

La sonda `p7Exception` y su copia en la prueba de P10 se reformatearon a líneas de 120 caracteres.

Prueba nueva: `Freeze30ContractTests::testCaughtExceptionNotCaptured`.
