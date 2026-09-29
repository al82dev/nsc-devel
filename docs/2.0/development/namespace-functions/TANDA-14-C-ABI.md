# Tanda 14: C ABI para funciones de namespace

## Objetivo

Permitir que una función libre sea una exportación C explícita de Windows x64 y que atraviese parser, semántica, LLVM, DLL, header y NSMETA sin convertirse en método.

## Sintaxis

La forma admitida es:

    public export extern C int ns_add(int left, int right)
    {
        return left + right;
    }

La secuencia export extern C es indivisible, exige cuerpo y exige accesibilidad public.

## Cambios

- El parser acepta export extern C sobre funciones de namespace y lo rechaza sobre otros elementos.
- C continúa siendo contextual, igual que en el bloque extern C existente.
- El modelo distingue función nativa, import C y export C.
- El nombre nativo interno permanece mangled; el nombre C plano pertenece al wrapper.
- Dos funciones no pueden reclamar el mismo símbolo C aunque estén en namespaces distintos.
- HIR y MIR transportan isExportedC y cAbiName.
- El lowerer LLVM existente genera el wrapper Windows x64 para la función libre.
- La DLL exporta el símbolo nativo público y, cuando corresponde, el wrapper C.
- El header de host incluye el prototipo y todos los tipos alcanzables desde su firma.
- NSMETA distingue export C de import C; un consumidor reconstruye el export como una declaración extern C directa.
- Las funciones públicas reconstruidas desde NSMETA se tratan como implementaciones nativas precompiladas y no exigen cuerpo local.
- Se corrigió la accesibilidad perdida en miembros de bloques extern C.

## Archivos modificados

- src/Parser/Parser.cpp
- include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h
- src/Frontend/ProjectNamespaceFunctionModel.cpp
- src/Frontend/ProjectOopHirModel.cpp
- src/Interop/CHeaderGenerator.cpp
- include/nativesharp/Interop/LibraryMetadata.h
- src/Interop/LibraryMetadata.cpp
- src/Driver/Driver.cpp
- CMakeLists.txt

## Archivo nuevo

- tests/unit/NamespaceFunctionCAbiTests.cpp: prueba aislada de todas las capas del contrato C y de colisiones planas.

## Mejora obtenida

Una API procedural puede exponerse directamente a C con una declaración legible y estable. La implementación NativeSharp conserva su identidad interna mientras el host recibe un símbolo C sin mangling y un header reproducible.

## Compatibilidad

- Target único: Windows x64.
- Native ABI 1.0 permanece sin cambios.
- Runtime ABI 1.0 permanece sin cambios.
- NSMETA 1.21 usa un bit aditivo dentro del registro introducido en la tanda 13.

## Verificación

- Construcción incremental exclusiva del objetivo nsc: correcta.
- Construcción exclusiva de nsc_namespace_function_c_abi_tests: correcta.
- Ejecución exclusiva de nsc_namespace_function_c_abi_tests: PASS, 8 verificaciones.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimizaciones.