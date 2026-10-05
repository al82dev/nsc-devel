# Corrección de baseline — compatibilidad `unsafe` por edición

## Español

El enforcement léxico de `unsafe` pertenece a la edición 3. El frontend lo aplicaba por error a proyectos edición 2 y rompía fixtures históricos de punteros crudos y FFI. Los ocho puntos de control ahora consultan la edición: V2 conserva su semántica anterior y V3 continúa exigiendo bloques auditables. El fixture `c56_ns3_null_with`, que sí es V3, declara explícitamente su región insegura. También se completó un inicializador C++ de `DiagnosticQuality` para mantener `-Werror`.

## English

Lexical `unsafe` enforcement belongs to edition 3. The frontend incorrectly applied it to edition 2 projects, breaking historical raw-pointer and FFI fixtures. All eight enforcement points now consult the edition: V2 preserves its previous semantics while V3 still requires auditable blocks. The `c56_ns3_null_with` fixture is genuinely V3 and now declares its unsafe region explicitly. A C++ `DiagnosticQuality` initializer was also completed to preserve `-Werror` builds.
