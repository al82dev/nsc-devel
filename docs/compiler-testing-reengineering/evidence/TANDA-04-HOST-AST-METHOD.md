# Evidencia de método — Tanda 04, Host.AST

## Universo leído

Se leyeron completos y en orden físico los diez archivos del componente:

| ID | Archivo | Líneas | Bytes | SHA-256 confirmado |
|---|---|---:|---:|---|
| PF-000079 | `include/nativesharp/AST/AstArena.h` | 39 | 778 | sí |
| PF-000080 | `include/nativesharp/AST/AstDumper.h` | 18 | 403 | sí |
| PF-000081 | `include/nativesharp/AST/AstNode.h` | 243 | 10,323 | sí |
| PF-000082 | `include/nativesharp/AST/AstNodeKind.h` | 172 | 7,353 | sí |
| PF-000083 | `include/nativesharp/AST/TypeSyntax.h` | 250 | 12,443 | sí |
| PF-000621 | `src/AST/AstArena.cpp` | 12 | 251 | sí |
| PF-000622 | `src/AST/AstDumper.cpp` | 256 | 9,469 | sí |
| PF-000623 | `src/AST/AstNode.cpp` | 120 | 5,811 | sí |
| PF-000624 | `src/AST/AstNodeKind.cpp` | 115 | 4,606 | sí |
| PF-000625 | `src/AST/TypeSyntax.cpp` | 147 | 6,645 | sí |

Total: 1,372 líneas y 58,082 bytes.

## Extracción y contraste

1. Se extrajeron las 61 definiciones `AstNode::` y las 61 definiciones `TypeSyntax::`; cada una se contrastó
   contra su declaración y se incorporó al inventario de símbolos.
2. Se extrajeron los 100 valores de `AstNodeKind` y se contrastaron contra los 100 retornos conocidos de
   `astNodeKindName` y su fallback.
3. Se inventariaron la arena, sus tipos borrados, los records auxiliares, los dos modelos de estado y las rutas
   del dumper; total: 155 símbolos.
4. Se recorrió el estado en ambas direcciones: de cada setter/append/clear hacia su almacenamiento y de cada
   campo hacia sus lectores, dumper y consumidores inmediatos.
5. Se separaron 20 contratos estables. Cada contrato tiene símbolos productores y casos lógicos diseñados.
6. Se diseñaron 230 casos: 217 de frecuencia PR y 13 NIGHTLY. Son filas parametrizables, no ejecutables.
7. Se contrastaron IDs, referencias, definiciones, huellas, paridad CSV/JSON y paridad Markdown/CSV.

## Fronteras revisadas

- construcción, registro, type erasure, ownership y destrucción LIFO de `AstArena`;
- falla de constructor frente a falla posterior de registro en `make`;
- ordinales, nombres y fallback del catálogo `AstNodeKind`;
- defaults, flags y punteros de records genéricos, lifetime, FP y FFI unwind;
- todos los canales escalares, de tipo, genéricos, lifetime y topológicos de `AstNode`;
- todos los canales nominales, tuple, función, const, pack, modifier y origen de `TypeSyntax`;
- nulls, duplicados, ciclos, inconsistencia permitida y ausencia de ownership en punteros AST;
- bytes, orden, indentación, campos incluidos y omisiones del dumper;
- recursión sin visited set ni límite de profundidad en el dumper.

## Límites del trabajo

No se cambió ni ejecutó código, pruebas, builders o infraestructura. No hubo build, baseline, coverage,
instalación, deploy, escritura en C: ni remotos. Los seis cambios preexistentes permanecieron fuera de este
trabajo.
