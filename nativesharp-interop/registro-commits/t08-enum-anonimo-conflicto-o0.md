# Conflicto de enum anónimo sin procedencia compartida

`plantillas/validar-enum-conflicto-o0.cmd` reutiliza el productor y dos
consumidores existentes. Cada consumidor importa una declaración anónima de
otra fuente y también usa la constante publicada por la biblioteca. NSC
diagnostica ambigüedad en ambas muestras, incluida la de contenido igual.
El control dirigido pasó en O0. No demuestra el comportamiento de todos los
enums anónimos ni reemplaza la validación completa de la tanda.
