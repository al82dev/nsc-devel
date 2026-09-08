# 155 — Handover de Stage 67 certificado a E12 / release-ready

**Fecha:** 2026-09-08

**Origen:** NativeSharp 0.1, `nsc 0.9.0`, Stage 67

**Destino recomendado:** rama nueva para E12

**Objetivo del siguiente chat:** cerrar la preparación técnica para un release candidate 1.0 sin
añadir superficie ajena al propósito de NativeSharp.

## 1. Estado que recibe la nueva rama

E11 está completa. No debe volver a implementarse ni presentarse como trabajo pendiente.

```text
G00:        20260908-092439-7d0bd312
estado:     LANDED
certified:  true
fallos:     0
S0 = S1:    7200c74938f0469350409444a4f327e18b093af9df9b958e041920a3649fd8a9
archivos:   1035
CTest:      304/304 lld-link + 304/304 NSLink
runner:     162/162 lld-link + 162/162 NSLink
total:      324/324 casos
```

Compilador instalado:

```text
nsc 0.9.0+build.20260908T152440Z.079cf744116ec119
NativeSharp 0.1, stage 67
SHA-256 f7e920496191650c460ff580ed49d05e0cbefb6356d685f2a7784c28b65668e6
```

La evidencia canónica vive en `_abi/gates/G00/gate.json` y
`_nscbuilder/history/20260908-092439-7d0bd312/`.

## 2. Qué entregó E11

La antigua captura por copia fue sustituida por clausuras léxicas reales:

- celdas compartidas entre el método creador y lambdas hermanas;
- mutación visible desde cualquier clausura que capture la misma declaración;
- lambdas anidadas y capturas transitivas;
- parámetros contextuales o explícitos;
- parámetros `ref`/`out`/`in` con validación de firma;
- tipos explícitos usados para resolver overloads de delegates;
- structs grandes y fixed arrays capturados con tamaño y alineación reales;
- retain/release atómico por celda;
- destructor específico del entorno;
- liberación del owner creador por retorno normal y por unwind;
- lambda sin captura convertible a `extern C delegate`;
- diagnóstico específico para una lambda C que intenta capturar estado.

La especificación normativa, la gramática y el mapa de conformidad ya reflejan estas reglas.

## 3. Diseño que no debe deshacerse

Una captura tiene dos identidades distintas:

1. la declaración original que representa;
2. el lugar desde donde la lambda actual obtiene su celda.

`MethodLambdaCaptureSourceKind` conserva esa diferencia mediante `OriginalLocal`,
`OriginalParameter`, `OriginalThis`, `ParentLocal`, `ParentParameter` y `ParentCapture`. Esta
separación es la que hace correctas las capturas transitivas y evita duplicarlas.

En HIR y MIR, las operaciones relevantes son:

- `CaptureCellReference`;
- `ClosureCaptureCellReference`;
- `ClosureCaptureAddress`.

En LLVM, el entorno contiene punteros a celdas, no copias de los valores. Cada celda tiene un
contador atómico de 64 bits y un payload alineado. El ABI público del delegate continúa siendo la
celda congelada de 32 bytes; E11 no cambió Native ABI 1.0 ni Runtime ABI 1.0.

## 4. Restricciones conscientes de lambdas

Estas reglas son deliberadas y no deben “corregirse” eliminando seguridad:

- una lambda necesita un delegate destino;
- un `extern C delegate` no admite contexto capturado;
- una captura no mueve ownership implícitamente;
- locales `raii`, préstamos, vistas y parámetros `owned` se rechazan porque el delegate podría
  sobrevivir su almacenamiento;
- el análisis actual es conservador: todavía no demuestra que una lambda concreta no escape;
- ligar un delegate directamente a un método de instancia con receptor struct sigue siendo una
  deuda separada del mecanismo de lambdas.

Si alguna de estas fronteras cambia, necesita regla normativa, caso rojo mínimo, diagnóstico,
HIR/MIR/LLVM inspeccionable y ejecución O0/O2. No basta con aceptar la sintaxis.

## 5. Archivos centrales de E11

### Sintaxis y semántica

- `src/Parser/Parser.cpp`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`

### Representación intermedia y backend

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/ProjectOopHirModel.cpp`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`

### Pruebas

- `tests/unit/Stage43LambdasClosuresTests.cpp`
- `tests/unit/Stage55XInteropFoundationTests.cpp`
- `tests/unit/Stage67E11LambdaExecutionTests.cpp`

### Contrato y documentación

- `spec/NativeSharp_Grammar_1_0.ebnf`
- `spec/NativeSharp_Language_Specification_1_0.md`
- `spec/NativeSharp_1_0_conformance.map`
- `mkdown/121_next_steps_for_the_1_0_glory.md`
- `mkdown/153_1_0_e11_lambdas_y_clausuras_lexicas_stage67.md`
- `mkdown/154_baseline_certificado_1_0_e11_stage67.md`

### Identidad y registro

- `include/nativesharp/Basic/Version.h`
- `CMakeLists.txt`
- `tests/unit/Cc1BuildIdentityTests.cpp`
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`

## 6. Pruebas que protegen E11

`nsc.stage43.lambdas-closures` protege binding, capturas, HIR, MIR, LLVM, O2 y COFF. La suite tiene
81 checks internos.

`nsc.stage67.e11-lambda-execution` construye y ejecuta a O0/O2 un programa que devuelve 42 y cubre:

- mutación compartida;
- lambda que devuelve lambda;
- parámetro `ref`;
- struct de 16 bytes;
- fixed array de 16 bytes;
- `delete` de delegates devueltos;
- continuidad de una celda después de un catch local;
- cleanup cuando una excepción abandona el método creador.

`nsc.stage55x.interop-foundation` protege la lambda C sin captura y el rechazo de una captura sin
canal de entorno.

Las suites Stage 45 de funclets y ejecución RAII deben acompañar cualquier cambio futuro al
lifetime de clausuras.

## 7. Estado del árbol al hacer el handover

La nueva rama debe partir del working tree actual, no del último commit conocido. La historia del
proyecto no garantiza un commit por cada entrega y los cambios de E11 viven en el árbol compartido.

Los directorios `build-clang`, `build-tests` y `build-test-lldlinker` contienen resultados generados
por compilaciones y por G00. No se deben restaurar, borrar ni incluir en una operación destructiva
sin comprobar antes qué evidencia espera conservar el usuario.

Después del `LANDED` se agregaron los documentos 154 y 155 y se actualizaron los índices. Por eso
el árbol actual ya no tiene por qué reproducir el digest S0/S1 histórico: ese digest identifica la
fotografía efectivamente certificada; las actas posteriores describen esa fotografía.

## 8. Siguiente bloque: E12

E12 debe ser una megaentrega de estabilización y release-readiness. Su primera tarea no es inventar
features, sino convertir los contratos ya existentes en una decisión mecánica de “listo para RC”.

Alcance recomendado:

1. Crear un gate `release-ready` que reúna las autoridades ya existentes sin duplicarlas.
2. Ejecutar un soak reproducible de los cuatro cuadrantes: dos enlazadores por O0/O2.
3. Añadir estrés concentrado de ownership, excepciones, clausuras y límites entre módulos.
4. Auditar fugas, doble destrucción y cleanup en caminos largos o combinados.
5. Consolidar manual del lenguaje, guía de ownership, guía FFI y guía de distribución.
6. Publicar una lista corta y honesta de limitaciones aceptadas.
7. Definir el periodo sin cambios semánticos previo al release candidate.
8. Producir una única megaentrega, su MD didáctico y después solicitar G00.

El primer paso del nuevo chat debe ser auditar qué piezas de este bloque ya existen en scripts,
contratos y documentos. No debe reescribir gates certificados ni declarar trabajo completado sólo
por encontrar un nombre parecido.

## 9. Reglas de trabajo para la nueva rama

- Entregas suficientemente grandes para cerrar bloques completos.
- Cada cambio genera o actualiza un MD didáctico en la misma pasada.
- El MD explica problema, diseño, métodos, archivos nuevos/modificados, pruebas y resultado.
- No ejecutar G00 mientras se está modificando el árbol; el usuario realiza la certificación.
- No declarar un baseline a partir de pruebas focales.
- No usar consumidores externos como gate obligatorio del compilador.
- Los reportes de bindings sirven para descubrir limitaciones, no para mezclar repositorios.
- Mantener el enfoque de lenguaje, compilador, runtime nativo, ABI, linker y bibliotecas nativas.
- No ampliar el producto hacia áreas fuera de su propósito.
- Preservar los cambios del usuario y no limpiar el working tree a ciegas.

## 10. Lectura mínima obligatoria del siguiente chat

En este orden:

1. `mkdown/155_handover_stage67_a_e12_release_ready.md`;
2. `mkdown/153_1_0_e11_lambdas_y_clausuras_lexicas_stage67.md`;
3. `mkdown/154_baseline_certificado_1_0_e11_stage67.md`;
4. `mkdown/152_1_0_gate_del_compilador_desacoplado_de_bindings_y_consumidores.md`;
5. secciones 9.5, 11, 12 y 13 de `mkdown/121_next_steps_for_the_1_0_glory.md`;
6. `_abi/gates/G00/gate.json`;
7. `spec/NativeSharp_Language_Specification_1_0.md` y el mapa de conformidad.

## 11. Condición de cierre de E12

E12 no termina al crear un script o al producir otro binario. Termina cuando existe una decisión
`release-ready` reproducible, explicable y adversarial que reúna:

- el baseline instalado vigente;
- las matrices completas;
- el soak de estabilidad;
- la ausencia de fallos críticos conocidos;
- la documentación pública coherente;
- el inventario de limitaciones aceptadas;
- evidencia suficiente para abrir el periodo de release candidate.

Sólo después se pide al usuario compilar, instalar y certificar la nueva fotografía.
