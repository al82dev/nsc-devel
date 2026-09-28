# Tanda 5: lookup de funciones de namespace

## Objetivo

Registrar, antes del análisis de tipos de argumentos, qué conjuntos de funciones de namespace puede nombrar cada llamada del proyecto.

## Cambios

- El modelo recorre todas las unidades después de consolidar símbolos y crea bindings estables por nodo `CallExpression`.
- Se soportan llamadas no calificadas dentro del namespace actual.
- Los `using namespace` del archivo aportan candidatos sólo cuando el namespace actual no contiene el nombre.
- Se soportan nombres calificados y alias de namespace.
- Cada binding conserva el nombre escrito, namespace léxico, archivo, proyecto y todos los ordinales candidatos.
- Los conjuntos de sobrecargas se preservan completos; esta fase no elige una firma antes de conocer los tipos de argumentos.
- La visibilidad incluye el proyecto actual y sus `ProjectReference`; desde referencias sólo se incorporan funciones `public`.
- El resultado es determinista y no depende del orden de archivos.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`: añade el contrato de reference bindings.
- `src/Frontend/ProjectNamespaceFunctionModel.cpp`: resuelve namespace actual, imports, aliases, calificación y visibilidad.
- `src/Frontend/ProjectCompilation.cpp`: entrega las unidades al modelo.
- `CMakeLists.txt`: registra la prueba focalizada.

## Archivos nuevos

- `tests/unit/NamespaceFunctionLookupTests.cpp`: cubre lookup actual, por using, calificado y mediante alias.
- Este documento.

## Mejora obtenida

Las llamadas de todos los archivos ya apuntan a conjuntos semánticos de funciones auténticas de namespace sin inventar miembros de clase. La siguiente fase puede aplicar resolución de sobrecargas con tipos y conversiones sobre candidatos previamente filtrados por alcance y visibilidad.

## Verificación específica

- Construcción incremental exclusiva de `nsc`: correcta.
- Construcción y ejecución exclusiva de `nsc_namespace_function_lookup_tests`.
- Resultado: `PASS: 9 checks`.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimización.