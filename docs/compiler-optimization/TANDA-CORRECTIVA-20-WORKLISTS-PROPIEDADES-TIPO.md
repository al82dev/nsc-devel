# Tanda correctiva 20: worklists para propiedades de tipo

## SCOPE LOCK

### Objetivo

Eliminar la recursion restante en las consultas de destructor transitivo, copyabilidad implicita y
move-only que se ejecutan repetidamente desde los shards de method bodies. Deben conservarse las
reglas actuales para escalares, funciones, punteros, arrays dinamicos, tuples, fixed arrays,
structs, campos `owned`, destructores y ciclos de tipos por valor.

### Archivos autorizados

- `src/Frontend/ProjectMemberModel.cpp`
- `tests/unit/ProjectMemberTypeWorklistTests.cpp` (nuevo)
- `CMakeLists.txt`
- `docs/compiler-optimization/TANDA-CORRECTIVA-20-WORKLISTS-PROPIEDADES-TIPO.md`

### Superficies protegidas

- Todas las pruebas, fixtures y proyectos historicos existentes.
- El parser, los diagnosticos, LLVM, NSMETA, runtime y bibliotecas.
- NSLink y todos los demas repositorios o componentes.
- `C:\native-sharp` y cualquier otra ruta de `C:`.
- `_tmp_strip_saru_exceptions.py`.

### Implementacion permitida

1. Sustituir exclusivamente los tres recorridos recursivos por worklists locales iterativos.
2. Mantener conjuntos locales de structs ya visitados para terminar ciclos sin duplicar trabajo.
3. Agregar una prueba nueva para structs, tuples y fixed arrays copy, move-only y destructibles.

### Implementacion prohibida

- Agregar caches mutables o estado compartido.
- Cambiar una regla, un diagnostico o una expectativa historica.
- Ejecutar baseline, minibaseline u optimizaciones O1/O2/O3.

### Validacion autorizada

- Compilar exclusivamente el nuevo ejecutable de prueba.
- Ejecutar directamente el nuevo ejecutable de prueba.
- Revisar `git diff --check` y el diff del commit.

## Estado

EN IMPLEMENTACION.

## Evidencia

Pendiente de la prueba puntual posterior al commit de implementacion.
