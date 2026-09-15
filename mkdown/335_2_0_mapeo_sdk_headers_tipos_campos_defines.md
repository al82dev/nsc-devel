# Ítem 335 — Mapeo de headers, tipos, campos y defines / Header, type, field, and define mapping

## Resultado / Result

La sección `Mappings` del manifiesto `.nssdk` declara cuatro clases de entrada: `Header`, `Type`,
`Field` y `Define`. Cada una conserva el nombre nativo y su identidad NativeSharp; los campos se
califican además por el tipo nativo que los contiene. / The `.nssdk` manifest `Mappings` section
declares four entry classes: `Header`, `Type`, `Field`, and `Define`. Each preserves the native name
and its NativeSharp identity; fields are additionally qualified by their containing native type.

## Determinismo y seguridad / Determinism and safety

El lector exige atributos completos, elementos vacíos y claves únicas por clase. No adivina
mapeos ni acepta clases desconocidas, por lo que un SDK mal escrito falla antes de compilar código
de usuario. / The reader requires complete attributes, empty elements, and unique per-class keys.
It neither guesses mappings nor accepts unknown classes, so a malformed SDK fails before user code
is compiled.

## Pruebas / Tests

La prueba dirigida verifica las cuatro clases, la preservación exacta de sus datos y el rechazo de
claves duplicadas y elementos no soportados. / The focused test verifies all four classes, exact
data preservation, and rejection of duplicate keys and unsupported elements.
