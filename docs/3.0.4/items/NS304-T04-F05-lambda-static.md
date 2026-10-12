# NS304-T04-F05 — Lambdas y funciones locales estáticas + prohibición de captura

- **Estado**: CERRADO (lambda `static`) / residual (función local `static`)
- **Rama**: development

## Resultado (validado)

- Parser reconoce `static` delante de lambda (`KwStatic`) y guarda `static` en el auxiliaryText.
- `bindLambdaToDelegate` rechaza captura en lambda `static`.
- **Prueba `TL-LANG-318`** (exit 42 + rechazo de captura): `static (int x) => x + 1` — **PASS**;
  `static (int x) => x + captured` se diagnostica.

## Residual

Función local `static int f() {...}` (declaración de función dentro de un método) aún no existe;
solo se cubrió el prefijo `static` de lambda.
