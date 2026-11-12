# Identidad de dueños de vistas bajo shadowing

## Caso y causa

Una vista construida con referencias a dueños exteriores se asignaba a otro
local dentro de un bloque que declaraba un dueño homónimo. Aunque los dueños
originales seguían vivos, el análisis volvía a resolver el texto `@nombre` en
el bloque de uso y confundía el símbolo original con el nuevo. El compilador
anterior a este cambio también rechazó el ejemplo válido: evidencia en
`A:/nsc-vistas-evidencia/view-shadow-k0tnhzj4/results.json`. La reproducción
de dos raíces y ramas anterior a esta corrección está en
`A:/nsc-vistas-evidencia/view-shadow-identity-ixb280n5/results.json`.

## Cambio y límites

Los locales conservan, además de los nombres de campos anteriores, identidades
de raíces locales o parámetros para construcciones de vistas mediante `ref`.
Al copiar una vista se conservan todas las identidades, incluso si comparten
nombre. La asignación valida cada dueño local por su ordinal semántico y solo
omite resolver de nuevo un nombre cuando no hay identidad desconocida para él.
El fusionador de resultados de callables desplaza los ordinales de locales;
los ordinales de parámetros permanecen relativos al callable.

Los productores heredados que no aportan identidad añaden `Unknown`. En ese
caso se conserva la comprobación por nombre, por lo que algunas variantes con
shadowing todavía pueden rechazarse conservadoramente. La información se une
sin borrar raíces en el modelo local, como los nombres existentes; no se afirma
cobertura de todas las formas de vista, llamadas, metadata ni alias. El marcador
de origen desconocido existente `*` no se elimina ni se convierte en prueba de
seguridad. Los archivos de analizadores, `MethodLocalSymbol.h` y el fusionador
son compartidos y pueden entrar en conflicto al integrar otras ramas.

## Verificación dirigida

El build incremental con headers separados terminó con código 0; la unidad
`ProjectCallableResultMergerOrdinalTests` informó `PASS: 12 checks`, incluidos
dos callables cuyos dueños y parámetros se verifican después de la fusión.
El nsc usado en la selección final tiene SHA-256
`6aa0125fb32d5e64c6dfa6cdd1d825b865f246b5eb045b4d66d5595e14e12ed0`.

Con `vistas-env.cmd` y `python tests/integration/view_shadow_identity_vistas.py
A:/nsc-vistas-evidencia/build/bin/nsc.exe`, los cuatro controles válidos
compilaron y retornaron 42 en O0 y O2; los tres escapes fueron rechazados con
diagnóstico de lifetime y no se ejecutaron. El log es
`A:/nsc-vistas-evidencia/view-shadow-identity-final.log` y los comandos,
fuentes y resultados están en
`A:/nsc-vistas-evidencia/temp/view-shadow-identity-ywwdvq9k/results.json`.
La copia exacta del fixture es `view-shadow-identity-fixture-prebuild.py`,
SHA-256 `16d15fc2376031da0e517e38945567da43c76344b247fca62863a0ea67c2c92d`.

La regresión `tuple_inline_view_lifetime_vistas.py` volvió a terminar 0 con siete
positivos en O0/O2 y tres negativos únicamente analizados; log
`A:/nsc-vistas-evidencia/tuple-inline-view-identity-final.log`. Estos resultados
corresponden a un build conjunto que también contenía cambios de FnOnce: no
certifican por sí solos un commit aislado ni toda la campaña. No se ejecutó una
batería completa ni se modificó interoperabilidad C/C++.
