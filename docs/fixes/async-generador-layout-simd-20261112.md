# Layout de elemento SIMD en generador async

**Estado: defecto reproducido; corrección pendiente.** El análisis dirigido
utilizó el compilador aislado `A:/nsc-fixes-evidencia/build/bin/nsc.exe`, con
SHA-256 `cd8ac03cd68262e604cad9bf6319552c886ae5ce2421a14313103bcf59bb1154`
antes y después. El HEAD observado durante la investigación fue
`466fb3f2f026b65503bfc0838b06e70db567e26f`; no se ha demostrado una
correspondencia exacta entre el hash de ese ejecutable y ese commit.

El proyecto original se conserva sin modificar en
`A:/nsc-fixes-evidencia/async-yield-simd-layout-repro-20261112/`. La ejecución
de `--check-project-oop-mir` devolvió código 1 y
`NativeSharp generator iterator has no physical layout:` para la clave
`async-iterator:simd<builtin:int,4>|type:Probe::NativeSharp.Core.Exception`.
Los comandos completos, hashes, fuentes y salidas están en
`A:/nsc-fixes-evidencia/async-simd-layout-directed-20261112.log` y la carpeta
homónima. No se generaron ni ejecutaron programas.

Los controles independientes aportan el siguiente contraste:

- Un generador async mínimo con `yield return value` y elemento
  `simd<int,4>` falla con el mismo diagnóstico, sin condición SIMD.
- Un generador equivalente con elemento `int` supera la comprobación MIR.
- Un `struct` que contiene un campo `simd<int,4>` supera la comprobación MIR.

Por inspección, `ProjectOopMirModel::appendIteratorLayout` en
`src/Frontend/MIR/MirLayoutImporter.cpp` solicita el layout del elemento
mediante `ProjectObjectLayoutModel::valueLayoutForTypeKey`. Esa consulta no
contiene un caso para la clave SIMD y retorna ausencia para este tipo, mientras
`ProjectObjectLayoutModel::layoutBaseType` sí reconoce SIMD a partir de la
sintaxis de tipo. Esto identifica una diferencia de rutas que explica el
diagnóstico observado; no demuestra que baste con añadir una condición ni que
otras representaciones o configuraciones sean correctas. Las superficies
consultadas describen `AsyncIterator<T,E>` sin una exclusión SIMD identificada;
no se ha acreditado que esta combinación esté fuera de contrato.

`src/Frontend/ProjectObjectLayoutModel.cpp` es un **archivo compartido con el
frente SIMD** y con otros consumidores de layout. Cualquier propuesta de
cambio requiere coordinación y pruebas dirigidas con ese frente. También deben
verificarse por separado tamaño, alineación, ABI y los consumidores afectados;
no se ha modificado la implementación ni se afirma que el defecto esté resuelto.
