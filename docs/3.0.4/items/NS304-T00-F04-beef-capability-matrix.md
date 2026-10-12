# NS304-T00-F04 — Matriz Beef por capacidad

## Estado

`CERRADO` el 2026-10-12 como matriz contractual. No declara paridad
funcional ni compatibilidad de sintaxis con Beef: clasifica capacidades de las
seis áreas comprometidas por F04 y asigna un dueño NS304 a cada gap.

## SCOPE LOCK

- Objetivo: comparar memoria, extensions, generics, comptime, interop y
  patterns por capacidad observable, no por parecido superficial.
- Naturaleza: diseño/auditoría documental; sin cambio de producto.
- Revisión base: `816ce1a88` en `development`.
- Archivos modificables: este expediente y el tracking maestro.
- Protegido: producto, runtime, CMake, toolchain, Builder, manifests y pruebas.
- Terminación: una disposición por capacidad, dueño para cada gap y
  divergencias explícitas donde NativeSharp conserva una frontera distinta.

## Fuentes y autoridad

- Guía oficial: [Beef language guide](https://www.beeflang.org/docs/language-guide/).
- Áreas oficiales: [memory](https://www.beeflang.org/docs/language-guide/memory/),
  [extensions](https://www.beeflang.org/docs/language-guide/datatypes/extensions/),
  [generics](https://www.beeflang.org/docs/language-guide/generics/),
  [comptime](https://www.beeflang.org/docs/language-guide/comptime/),
  [interop](https://www.beeflang.org/docs/language-guide/interop/) y
  [patterns](https://www.beeflang.org/docs/language-guide/patterns/).
- Contraste local: `A:\vsBeef.md`, subordinado al documento maestro y al plan
  canónico `A:\NATIVESHARP-V3.0.4-PLAN-DE-DESARROLLO.md`.
- Frontera: la enumeración cubre las capacidades materiales documentadas en
  esas seis áreas; no pretende inventariar el resto del lenguaje Beef.

## Leyenda contractual

- `EQUIVALENTE`: NativeSharp ofrece o compromete una solución directa al mismo
  problema, aunque la ortografía pueda diferir.
- `ALTERNATIVA`: NativeSharp resuelve el propósito con otro modelo explícito.
- `DIVERGENCIA`: se rechaza deliberadamente el contrato Beef por ownership,
  layout, ABI, determinismo o ausencia de runtime administrado.
- `GAP_OBLIGATORIO`: el plan 3.0.4 compromete la capacidad y esta fila enlaza
  el ítem responsable; no se presenta como productiva hasta su cierre.

## Matriz exhaustiva de las seis áreas

| Área | Capacidad Beef | Disposición NativeSharp | Evidencia / dueño 3.0.4 |
|---|---|---|---|
| Memoria | Reserva local con `scope` | `GAP_OBLIGATORIO` | NS304-T13-F03 |
| Memoria | Selección de allocator global por workspace | `DIVERGENCIA` | se evita política global oculta; allocator y procedencia deben ser explícitos |
| Memoria | Allocator personalizado al construir objetos | `GAP_OBLIGATORIO` | NS304-T13-F01/F02 |
| Memoria | Propagación de allocator a contenedores y subreservas | `GAP_OBLIGATORIO` | NS304-T13-F04 |
| Memoria | Liberación manual con procedencia del deallocator | `GAP_OBLIGATORIO` | NS304-T13-F07 |
| Memoria | Append allocation / almacenamiento contiguo posterior al objeto | `GAP_OBLIGATORIO` | NS304-T13-F06 |
| Memoria | Destrucción determinista separada de la liberación | `ALTERNATIVA` | RAII/drop explícito más procedencia del allocator, sin `delete` polimórfico implícito |
| Memoria | Boxing implícito temporal en stack | `DIVERGENCIA` | `A:\vsBeef.md`: no heap/boxing implícito; costo y lifetime visibles |
| Memoria | Boxing explícito para borrado de tipo | `ALTERNATIVA` | representación explícita `dyn Trait`/objeto de interfaz, sin conversión silenciosa |
| Memoria | `Variant` no asignante con tag y payload | `EQUIVALENTE` | unions/variants etiquetados y patterns NativeSharp |
| Memoria | Allocator de depuración y detección de fugas | `DIVERGENCIA` | responsabilidad de tooling/runtime; no semántica lingüística 3.0.4 |
| Memoria | Fallo observable de asignación | `GAP_OBLIGATORIO` | NS304-T13-F08 |
| Memoria | Construcción in-place y rollback parcial | `GAP_OBLIGATORIO` | NS304-T13-F05 |
| Memoria | Reporte de costo por asignación, copia y movimiento | `GAP_OBLIGATORIO` | NS304-T13-F09 |
| Extensions | Métodos de extensión | `EQUIVALENTE` | vertical actual NS3-T05-F06; T17 conserva prueba terminal |
| Extensions | Extensiones condicionadas por restricciones genéricas | `GAP_OBLIGATORIO` | NS304-T08-F01/F06 |
| Extensions | Propiedades de extensión | `GAP_OBLIGATORIO` | NS304-T08-F02 |
| Extensions | Miembros estáticos de extensión | `GAP_OBLIGATORIO` | NS304-T08-F03 |
| Extensions | Operadores definidos por extensión | `GAP_OBLIGATORIO` | NS304-T08-F04 |
| Extensions | Conformidad externa a interfaces | `GAP_OBLIGATORIO` | NS304-T08-F05 |
| Extensions | Visibilidad por imports/dependencias | `GAP_OBLIGATORIO` | NS304-T08-F06 |
| Extensions | Persistencia binaria y versionado de extensiones | `GAP_OBLIGATORIO` | NS304-T08-F07 |
| Extensions | Añadir campos y cambiar layout desde una extensión | `DIVERGENCIA` | prohibido por ABI/layout estable |
| Extensions | Añadir constructores, destructores o inicializadores de campos externos | `DIVERGENCIA` | no se permite mutar lifetime/layout de un tipo ajeno |
| Extensions | Sobrescribir virtuales desde una extensión | `DIVERGENCIA` | no se permite mutar vtable/conformidad ABI fuera del dueño del tipo |
| Generics | Tipos y métodos genéricos especializados | `EQUIVALENTE` | verticales genéricas actuales, monomorfización y cierre T17 |
| Generics | Constraints por interfaces/traits | `EQUIVALENTE` | NS3-T04-F01/F05 |
| Generics | Constraints por clase base, struct o constructor | `ALTERNATIVA` | conjunto declarativo NativeSharp sin heredar semántica CLR |
| Generics | Especialización en compile time | `EQUIVALENTE` | monomorfización NativeSharp |
| Generics | Inferencia de argumentos | `EQUIVALENTE` | NS3-T04-F04 |
| Generics | Especialización condicional y control de solapes | `EQUIVALENTE` | NS3-T04-F06; T17 reconfirma coherencia |
| Generics | Genéricos de valor/const | `EQUIVALENTE` | NS3-T04-F02 |
| Generics | Packs de tipos y valores | `EQUIVALENTE` | NS3-T04-F03 |
| Generics | Límites de instanciación y diagnóstico de explosión | `EQUIVALENTE` | NS3-T04-F07 |
| Generics | Tipos ref-like como argumentos genéricos | `GAP_OBLIGATORIO` | NS304-T11-F05 |
| Comptime | Evaluación de constantes tipadas | `EQUIVALENTE` | `ComptimeEngine` y verticales consteval actuales |
| Comptime | Ejecutar funciones puras ordinarias al compilar | `EQUIVALENTE` | consteval/comptime tipado existente; T17 aporta cierre terminal |
| Comptime | Funciones exclusivamente compile-time | `ALTERNATIVA` | capabilities y restricciones declarativas, no segundo lenguaje implícito |
| Comptime | Evaluaciones aisladas y reproducibles | `ALTERNATIVA` | modelo determinista/capability-based en lugar de ambiente de host irrestricto |
| Comptime | Enumerar declaraciones por reflexión | `GAP_OBLIGATORIO` | NS304-T14-F01 |
| Comptime | Generar miembros, implementaciones y helpers tipados | `GAP_OBLIGATORIO` | NS304-T14-F02 |
| Comptime | Mixins textuales/call-site sin higiene | `DIVERGENCIA` | `A:\vsBeef.md`: generación estructurada, no pegado textual |
| Comptime | Macros/mixins de AST con higiene | `GAP_OBLIGATORIO` | NS304-T14-F03 |
| Comptime | Source mapping y backtrace de expansión | `GAP_OBLIGATORIO` | NS304-T14-F04 |
| Comptime | Generar genéricos, associated items, parciales y extensiones | `GAP_OBLIGATORIO` | NS304-T14-F05 |
| Comptime | Entradas externas declaradas, capabilities y cache determinista | `GAP_OBLIGATORIO` | NS304-T14-F06 |
| Comptime | Detección de ciclos, límites y presupuestos | `GAP_OBLIGATORIO` | NS304-T14-F07 |
| Comptime | Derives reales para serializer/formatter/bindings | `GAP_OBLIGATORIO` | NS304-T14-F08 |
| Interop | Representación C explícita para structs/unions | `EQUIVALENTE` | ABI Win64 y agregados C-compatibles actuales |
| Interop | Calling conventions y function pointers | `EQUIVALENTE` | NS3-T08-F01 |
| Interop | Import/export estático mediante ABI C | `EQUIVALENTE` | FFI NativeSharp/NSLink existente; T17 valida terminalmente |
| Interop | Import dinámico por atributo de DLL | `ALTERNATIVA` | loader/binding explícito de biblioteca, no resolución mágica del compilador |
| Interop | Ownership de handles, buffers y callbacks | `GAP_OBLIGATORIO` | NS304-T16-F04 |
| Interop | Metadatos estables entre módulos | `GAP_OBLIGATORIO` | NS304-T16-F01/F02/F03 |
| Interop | ABI general estable entre módulos de lenguaje | `ALTERNATIVA` | NativeSharp compromete NSMETA/ABI Win64 versionados donde Beef documenta límites de estabilidad |
| Interop | Extensiones que alteran layout a través de una frontera ABI | `DIVERGENCIA` | layout cerrado y verificable |
| Interop | Allocator global implícito compartido entre módulos | `DIVERGENCIA` | allocator/procedencia explícitos |
| Interop | Lifetime y afinidad de hilo para callbacks | `GAP_OBLIGATORIO` | NS304-T16-F04 |
| Interop | Interop C++ directo dependiente de ABI del proveedor | `DIVERGENCIA` | se conserva frontera C/ABI estable o binding explícito |
| Patterns | Patterns de tupla | `EQUIVALENTE` | NS3-T03-F02 |
| Patterns | Patterns de enum/union etiquetada | `EQUIVALENTE` | NS3-T03-F01/F03 |
| Patterns | Capturas y descartes | `EQUIVALENTE` | bindings de pattern actuales |
| Patterns | `switch` como expresión de pattern | `ALTERNATIVA` | `match` exhaustivo NativeSharp |
| Patterns | Exhaustividad e inalcanzables | `EQUIVALENTE` | checker de patterns actual; T17 conserva diagnóstico terminal |
| Patterns | Patterns de lista/slice | `GAP_OBLIGATORIO` | NS304-T03-F01/F02/F03 |
| Patterns | Patterns anidados, de propiedad, relacionales y lógicos completos | `GAP_OBLIGATORIO` | NS304-T03-F04 |
| Patterns | Binding ownership-aware (`copy`/`move`/`ref`/`ref mut`) | `GAP_OBLIGATORIO` | NS304-T03-F06 |
| Patterns | Lowering sin temporales ni asignaciones ocultas | `GAP_OBLIGATORIO` | NS304-T03-F07 |
| Patterns | Diagnósticos de exhaustividad, subsunción y binding | `GAP_OBLIGATORIO` | NS304-T03-F08 |

## Conteos

- Capacidades por área: memoria=14, extensions=11, generics=10,
  comptime=13, interop=11, patterns=10.
- Disposiciones: `EQUIVALENTE`=19, `ALTERNATIVA`=8,
  `DIVERGENCIA`=10, `GAP_OBLIGATORIO`=32.
- Total clasificado: **69/69**; omitidas=0; duplicadas=0;
  disposiciones múltiples=0; gaps sin dueño=0.

## Decisiones que no pueden degradarse a “paridad sintáctica”

1. No se adopta heap, boxing ni allocator global implícito.
2. Una extensión no puede alterar layout, lifetime o vtable de un tipo ajeno.
3. Comptime debe generar estructuras tipadas e higiénicas; no texto arbitrario.
4. Interop se apoya en contratos versionados y una frontera ABI explícita.
5. Patterns deben conservar ownership y costos observables durante el lowering.

## Gate F04

```text
areas_comprometidas = 6
capacidades_clasificadas = 69
omitidas = 0
duplicadas = 0
disposiciones_multiples = 0
gaps_sin_dueño = 0
```

Siguiente acción determinista: `NS304-T00-F05`, disposición
`PlannedFor304` sin presentar features pendientes como productivas.
