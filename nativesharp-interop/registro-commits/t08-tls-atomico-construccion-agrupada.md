# Construcción agrupada y pruebas internas atómicas

La construcción incremental de `nsc`, `nsc_cpp_session_tests` y
`nsc_project_oop_lowering_tests` pasó después de corregir la ubicación del
indicador en la identidad HIR. La suite de sesión de Clang pasó con la captura
atómica y el rechazo del codec anterior. El control focalizado
`mir-cache-restore-depth` pasó; se ejecutó desde A:.

La ejecución completa de `nsc_project_oop_lowering_tests` con el runtime de A:
informó **25 fallos de 652 comprobaciones**, incluyendo pruebas de operadores,
agregados y construcción iterativa. El gate anterior solo ejecutaba el foco de
caché, por lo que ese resultado anterior no demuestra que la suite completa
pasara. La causa y si son regresiones no están determinadas. Registro:
`A:/nsc-interop-build/tanda08-atomic-lowering-test-A.log`. La primera ejecución
también buscó un runtime en C:, corregido mediante el entorno de A: antes de la
segunda; no se escribió ni desplegó en C:.

La proyección de `_Atomic` sigue deshabilitada y el control positivo TLS
atómico sigue fallando. Faltan operaciones, barreras de escape, metadata y la
validación completa de la tanda.
