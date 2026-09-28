# Tanda 4: firmas, sobrecargas y combinación de declaraciones

## Objetivo

Convertir las declaraciones multiparchivo registradas en la tanda 3 en símbolos semánticos estables, con firma canónica y una única definición opcionalmente precedida por declaraciones anticipadas.

## Cambios

- Se añadió `ProjectNamespaceFunctionModel`, separado de `ProjectMemberModel`, para no representar funciones de namespace como miembros de una clase sintética.
- Cada símbolo conserva módulo, namespace, accesibilidad, retorno, ownership, parámetros, modos `ref`/`out`/`in`, variadicidad y convención externa.
- La firma usa tipos canónicos y distingue ownership y modo de paso; el tipo de retorno no forma parte de la identidad de sobrecarga.
- Declaraciones compatibles de archivos distintos se combinan en un símbolo y se conservan todas sus partes fuente.
- Se añadió un índice determinista por proyecto, namespace y nombre para obtener el conjunto completo de sobrecargas.
- Se diagnostican definiciones duplicadas, declaraciones incompatibles y declaraciones ordinarias que nunca reciben cuerpo.
- Los diagnósticos de conflicto incluyen una nota hacia la declaración anterior.
- La construcción del modelo se integró al final de la fase de miembros, cuando los tipos canónicos ya están disponibles.

## Archivos modificados

- `CMakeLists.txt`: registra el nuevo modelo y su prueba focalizada.
- `include/nativesharp/Diagnostics/DiagnosticId.h`: añade `MissingFunctionDefinition`.
- `include/nativesharp/Frontend/ProjectCompilation.h`: expone y posee el nuevo modelo.
- `src/Frontend/ProjectCompilation.cpp`: construye el modelo después de los miembros.

## Archivos nuevos

- `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h`: contrato público de símbolos y partes de declaración.
- `src/Frontend/ProjectNamespaceFunctionModel.cpp`: construcción de firmas, combinación y diagnósticos.
- `tests/unit/NamespaceFunctionSignatureTests.cpp`: prueba aislada de la tanda.
- Este documento.

## Por qué se crearon archivos nuevos

El índice sintáctico de nombres no debe asumir responsabilidades de semántica de firmas, y el modelo de miembros de clase no debe falsear el propietario de una función libre. Un modelo dedicado mantiene esas fronteras y deja una identidad reutilizable para lookup, cuerpos, HIR, MIR, LLVM y metadata.

## Mejora obtenida

Las funciones definidas o anticipadas en varios archivos ya convergen en una identidad semántica independiente del orden físico. Las fases posteriores pueden resolver llamadas contra sobrecargas reales y bajar exactamente una definición, con errores deterministas antes del análisis de cuerpos.

## Verificación específica

- Construcción incremental exclusiva del objetivo `nsc`: correcta.
- Construcción y ejecución exclusiva de `nsc_namespace_function_signature_tests`.
- Resultado: `PASS: 15 checks`.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimizaciones.