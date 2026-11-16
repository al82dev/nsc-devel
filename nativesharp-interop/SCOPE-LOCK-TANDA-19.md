# Alcance de la tanda 19

Base: `73673726b`. [Auditoría inicial](TANDA-19-AUDITORIA-INICIAL.md). Tareas: sección 19 de TRACKING
([pendientes](PENDIENTES-INTEROP-C-CPP.md)).

Cambios, en este orden:

1. `null` en punteros a función de convención extranjera, en asignación, argumento y comparación (T19-002).
2. Sobrecargas C++ ordenadas por la conversión C++ de cada argumento aritmético, como Clang (T19-006).
3. Retornos `const T&` de funciones C++ como referencias de solo lectura (T19-007).
4. Diagnóstico de acceso para miembros C++ privados y protegidos (T19-012).
5. Campos de `union` C++ (T19-004).
6. Alias de namespace como prefijo de tipo (T19-005).
7. Validación agrupada.

Cada cambio se compara con un control de Clang 23.1.0, en estática y DLL, O0–O3. Un rechazo que deje de serlo
se convierte en una comparación positiva y se conserva un rechazo genuino aparte.

Las demás tareas de la sección 19 siguen abiertas tras esta tanda. Sin modificar Clang ni NSLink, sin desplegar
en `C:` y con un único contrato NSMeta.

Presupuesto inicial:

- `ProjectTypeConversion.cpp` y `CallAnalyzer.cpp`;
- `CppSemanticFunction.cpp` y `NamespaceFunctionSymbol.h`;
- la importación de records C++ y la búsqueda de nombres;
- fixtures `pruebas-tanda-19-*` y documentación.
