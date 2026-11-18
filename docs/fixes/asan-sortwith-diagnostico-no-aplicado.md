# Fragmento diagnóstico propuesto para el caso 100 — NO APLICADO

Este fragmento es solo para que el responsable del frente de genéricos lo
evalúe y, si corresponde, lo use en su propia copia y build aislados. No se
aplicó a `A:/nsc-limitaciones-asan`, no fue compilado ni ejecutado y no es una
corrección de producto. La reproducción y los controles permanecen intactos.

En `src/Frontend/Generics/GenericTypeInference.cpp`, agregar temporalmente
`#include <cstdio>` y colocar este bloque en
`ProjectGenericModel::inferTypeArguments`, inmediatamente después de
`if (pattern == nullptr || actual == nullptr) return false;`:

```cpp
    if (record.name == "SortWith" && record.ownerName == "Algorithm")
    {
        const auto firstName = [](const ast::TypeSyntax* type) -> const char* {
            if (type->typeArguments().empty()) return "<none>";
            const ast::TypeSyntax* first = type->typeArguments().front();
            return first == nullptr ? "<null>" : first->baseName().c_str();
        };
        std::fprintf(stderr,
            "SORTWITH_INFER key=%s pattern=%s token=%d argc=%zu first=%s mods=%zu "
            "actual=%s token=%d argc=%zu first=%s mods=%zu\n",
            record.canonicalKey.c_str(), pattern->baseName().c_str(),
            static_cast<int>(pattern->baseTokenKind()), pattern->typeArguments().size(),
            firstName(pattern), pattern->modifiers().size(), actual->baseName().c_str(),
            static_cast<int>(actual->baseTokenKind()), actual->typeArguments().size(),
            firstName(actual), actual->modifiers().size());
        std::fflush(stderr);
    }
```

El bloque no modifica explícitamente argumentos, resultado ni selección de
candidatos; el logging sí altera salida, tiempos y ejecución concurrente, y no
constituye evidencia de equivalencia formal. El filtro limita el ruido al
nombre y dueño indicados, pero puede registrar otros `Algorithm.SortWith` con
ese nombre. `key` ayuda a distinguirlos. Cada invocación recursiva puede emitir
una línea; si el tipo exterior se rechaza, no habrá recorrido de sus hijos.
`first` muestra solo el primer argumento inmediato, no el árbol completo ni
una identidad resuelta. Un ordinal o nombre impreso no demuestra por sí mismo
que dos tipos son iguales: se requiere resolver su identidad en su contexto.
Los mensajes de workers simultáneos pueden intercalarse; usar `-j 1` y guardar
stdout/stderr y código de salida en rutas independientes de A.

Probar sobre el mismo productor y configuración con temporales en A:

1. El ejemplo original intacto con Core y Collections binarios; registrar las
   líneas para el parámetro `Span<T>` y el resultado de `Spans.Of(values)`.
2. El mismo ejemplo con Collections desde fuente y, por separado, los controles
   `Span<int>` y `NativeSharpCore::NativeSharp.Core.Span<int>` inicializados
   con `Spans.Of<int>(values)`; sus entradas y comandos están en
   `A:/nsc-asan-sdk-evidencia/sortwith-local-explicit-of-controls.json`.
3. Confirmar que los diagnósticos y códigos de cada control coinciden con los
   registrados antes de instrumentar. No declarar resuelta la inferencia por
   ver una línea ni convertir los controles en sustitutos del ejemplo original.
4. Al confirmar una hipótesis, añadir controles con tipos homónimos distintos,
   identidades equivalentes, ambigüedad y argumento explícito, y determinar la
   capa causal antes de preparar una corrección. No eliminar prefijos por texto.

La compilación del bloque, la disponibilidad exacta del candidato y la salida
real están **sin verificar**. El responsable debe retirar el fragmento de su
copia diagnóstica después de capturar evidencia y no incorporarlo a un cambio
de producto. Las rutas de trabajo y archivos de propiedad de otros chats están
descritos en `asan-sortwith-binario-handoff.md` junto a este documento.
