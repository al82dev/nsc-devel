# NativeSharp 2.0 — Items finales / Final items

**Estado:** plan de trabajo, todavía no implementado

**Base:** NativeSharp 1.0.1-dev

**Objetivo:** cerrar la evolución técnica necesaria antes de promover NativeSharp 2.0

**Progreso:** Tanda 1 cerrada y congelada en `nativesharp-2.0-tanda-01`; tanda 2 en ejecución

## Regla de ejecución

- Cada punto corresponde a un commit independiente.
- Cada commit incluirá implementación, pruebas dirigidas y un Markdown bilingüe de ingeniería.
- Después de cada commit se reconstruirá el compilador desde cero.
- No se ejecutará el baseline completo durante las entregas intermedias.
- Se ejecuta exactamente un baseline al final de cada tanda: 16 baselines en total.
- Los números siguientes son identificadores de planificación; todavía no representan commits creados.

## Tanda 1 — Acelerar el ciclo de desarrollo

1. **246 — Perfil detallado de compilación / Detailed compilation profiling — COMPLETADO / COMPLETE**
2. **247 — Eliminar trabajo innecesario en O0 / Remove unnecessary O0 work — COMPLETADO / COMPLETE**
3. **248 — Presupuesto de optimización O2 / O2 optimization budget — COMPLETADO / COMPLETE**
4. **249 — Control `-j N` y núcleos reservados / Job and reserved-core controls — COMPLETADO / COMPLETE**
5. **250 — Pool interno de workers del compilador / Internal compiler worker pool — COMPLETADO / COMPLETE**
6. **251 — Scheduler determinista sobre el grafo existente / Deterministic graph scheduler — COMPLETADO / COMPLETE**
7. **252 — ProjectReferences en paralelo / Parallel project references — COMPLETADO / COMPLETE**
8. **253 — Lectura, lexer y parser en paralelo / Parallel reading, lexing, and parsing — COMPLETADO / COMPLETE**
9. **254 — Índices y símbolos por archivo en paralelo / Parallel per-file symbol indexing — COMPLETADO / COMPLETE**
10. **255 — Construcción HIR de funciones en paralelo / Parallel function HIR construction — COMPLETADO / COMPLETE**
11. **256 — Construcción MIR de funciones en paralelo / Parallel function MIR construction — COMPLETADO / COMPLETE**
12. **257 — Partición del módulo en unidades de código / Module code partitioning — COMPLETADO / COMPLETE**
13. **258 — Generación LLVM/OBJ paralela / Parallel LLVM and object generation — COMPLETADO / COMPLETE**
14. **259 — Optimización O2 paralela / Parallel O2 optimization — COMPLETADO / COMPLETE**
15. **260 — Unión determinista de NSMETA / Deterministic NSMETA merging — COMPLETADO / COMPLETE**
16. **261 — Diagnósticos deterministas en paralelo / Deterministic parallel diagnostics — COMPLETADO / COMPLETE**
17. **262 — Cache de artefactos por contenido / Content-addressed artifact cache — COMPLETADO / COMPLETE**
18. **263 — Invalidación incremental precisa / Precise incremental invalidation — COMPLETADO / COMPLETE**
19. **264 — Perfiles Debug y Release / Debug and Release profiles — COMPLETADO / COMPLETE**

## Tanda 2 — Fundamentos del lenguaje

20. **265 — `static_assert` / Static assertions — COMPLETADO / COMPLETE**
21. **266 — `offsetof` / Field offsets — COMPLETADO / COMPLETE**
22. **267 — Arreglos multidimensionales fijos `T[3, 4]` / Fixed multidimensional arrays — COMPLETADO / COMPLETE**
23. **268 — Indexado `array[i, j]` / Multidimensional indexing — COMPLETADO / COMPLETE**
24. **269 — Arreglos locales variables `T[length]` / Variable-length local arrays — COMPLETADO / COMPLETE**
25. **270 — VLAs multidimensionales `T[rows, columns]` / Multidimensional VLAs — COMPLETADO / COMPLETE**
26. **271 — Seguridad, overflow y duración de VLAs / VLA safety, overflow, and lifetime — COMPLETADO / COMPLETE**
27. **272 — Alineación explícita por campo / Explicit per-field alignment — COMPLETADO / COMPLETE**
28. **273 — Almacenamiento trailing/flexible / Trailing and flexible storage — COMPLETADO / COMPLETE**
29. **274 — `readonly struct` / Readonly structures — COMPLETADO / COMPLETE**
30. **275 — Receptores readonly / Readonly receivers — COMPLETADO / COMPLETE**
31. **276 — Alias genéricos / Generic aliases — COMPLETADO / COMPLETE**
32. **277 — Inferencia genérica local / Local generic inference — COMPLETADO / COMPLETE**

## Tanda 3 — Inlining

33. **278 — Contrato semántico de `inline` / Inline semantic contract**
34. **279 — Inlining automático basado en costo / Cost-based automatic inlining**
35. **280 — Modificador `inline` / Inline modifier**
36. **281 — Modificador `forceinline` / Force-inline modifier**
37. **282 — Modificador `noinline` / No-inline modifier**
38. **283 — Inlining de métodos genéricos / Generic method inlining**
39. **284 — Inlining de structs e interfaces / Struct and interface inlining**
40. **285 — Inlining entre bibliotecas precompiladas / Cross-library inlining**
41. **286 — PDB y stepping de funciones inline / Inline PDB and stepping**

## Tanda 4 — Ensamblador integrado

42. **287 — Gramática de `asm {}` / Integrated assembly grammar**
43. **288 — Lexer y ubicaciones fuente de asm / Assembly lexer and source locations**
44. **289 — Operandos de entrada / Assembly input operands**
45. **290 — Operandos de salida / Assembly output operands**
46. **291 — Restricciones de registros / Register constraints**
47. **292 — Inmediatos y operandos de memoria / Immediate and memory operands**
48. **293 — Clobbers de registros / Register clobbers**
49. **294 — Clobbers de flags y memoria / Flag and memory clobbers**
50. **295 — Conservación de stack y registros no volátiles / Stack and nonvolatile-register safety**
51. **296 — Saltos y control de flujo / Assembly control flow**
52. **297 — Validación x64 y características de CPU / x64 and CPU-feature validation**
53. **298 — Representación de asm en MIR / Assembly MIR representation**
54. **299 — Lowering LLVM de asm / Assembly LLVM lowering**
55. **300 — Barreras de optimización alrededor de asm / Assembly optimization barriers**
56. **301 — PDB y stepping dentro de asm / Assembly PDB and stepping**
57. **302 — Certificación ABI C/C++ de asm / C and C++ assembly ABI certification**

## Tanda 5 — Atomics y memoria de sistema

58. **303 — Órdenes de memoria públicas / Public memory orders**
59. **304 — Load/store atómicos ordenados / Ordered atomic loads and stores**
60. **305 — Operaciones RMW ordenadas / Ordered read-modify-write operations**
61. **306 — Compare-exchange con órdenes de éxito y fallo / Ordered compare-exchange**
62. **307 — Fences de compilador y CPU / Compiler and CPU fences**
63. **308 — Memoria `volatile` para MMIO / Volatile MMIO memory**
64. **309 — Atomics de 64 bits / 64-bit atomics**
65. **310 — Atomics de punteros / Pointer atomics**
66. **311 — API pública `Interlocked` / Public Interlocked API**

## Tanda 6 — Referencias, placement y heap

67. **312 — Variables locales `ref` / Reference locals**
68. **313 — Retornos `ref` / Reference returns**
69. **314 — Indexadores `ref` / Reference-returning indexers**
70. **315 — Analizador unificado de escapes / Unified escape analysis**
71. **316 — Análisis de duración de referencias / Reference lifetime analysis**
72. **317 — Sintaxis de placement construction / Placement construction syntax**
73. **318 — Lowering de placement construction / Placement construction lowering**
74. **319 — Destrucción explícita sin liberar almacenamiento / Explicit destruction without deallocation**
75. **320 — Separar propiedad del objeto y almacenamiento / Separate object and storage ownership**
76. **321 — Contextos `noheap` / No-heap contexts**
77. **322 — Propagación de contratos `noheap` / No-heap contract propagation**
78. **323 — Escape analysis de clases hacia stack / Stack allocation of nonescaping classes**

## Tanda 7 — Layout C y MSVC

79. **324 — Structs anónimos / Anonymous structs**
80. **325 — Unions anónimos / Anonymous unions**
81. **326 — Promoción y resolución de miembros anónimos / Anonymous-member promotion**
82. **327 — Sintaxis de bitfields / Bit-field syntax**
83. **328 — Layout de bitfields MSVC / MSVC bit-field layout**
84. **329 — Lectura y escritura de bitfields / Bit-field access**
85. **330 — Bitfields volatile / Volatile bit fields**
86. **331 — Declaraciones C variádicas / Variadic C declarations**
87. **332 — Promociones C para varargs / C vararg promotions**
88. **333 — ABI Win64 de varargs / Win64 variadic ABI**

## Tanda 8 — SDK, COM y ABI

89. **334 — Formato declarativo de SDK / Declarative SDK format**
90. **335 — Mapeo de headers, tipos, campos y defines / Header, type, field, and define mapping**
91. **336 — Selección por arquitectura y versión / Architecture and version selection**
92. **337 — Diagnósticos de SDK y librerías faltantes / SDK and missing-library diagnostics**
93. **338 — Consumo COM mediante vtables crudas / Raw-vtable COM consumption**
94. **339 — Producción de vtables COM / COM vtable production**
95. **340 — Function pointers y delegates para COM / COM function pointers and delegates**
96. **341 — Thunks COM con excepciones contenidas / Exception-safe COM thunks**
97. **342 — Matriz ABI de structs y agregados / Struct and aggregate ABI matrix**
98. **343 — ABI de callbacks y parámetros de salida / Callback and output-parameter ABI**
99. **344 — `vectorcall` / Vectorcall**
100. **345 — HVA y retornos vectoriales / Homogeneous vector aggregates**
101. **346 — Segundo target guiado por consumidor real / Consumer-driven additional target**

## Tanda 9 — SIMD y CPU

102. **347 — Vectores enteros de 128 bits / 128-bit integer vectors**
103. **348 — Vectores float/double de 256 bits / 256-bit floating vectors**
104. **349 — Vectores enteros de 256 bits / 256-bit integer vectors**
105. **350 — Loads/stores SIMD alineados / Aligned SIMD loads and stores**
106. **351 — Loads/stores SIMD no alineados / Unaligned SIMD loads and stores**
107. **352 — `float2` y `float3` / Compact float vectors**
108. **353 — Matrices nativas / Native matrices**
109. **354 — Tipo `half` / Half-precision type**
110. **355 — Formatos GPU empaquetados / Packed GPU formats**
111. **356 — Swizzle y shuffle / Swizzle and shuffle**
112. **357 — Dot, cross, min, max y clamp / Vector math operations**
113. **358 — Sqrt, rsqrt y FMA / Vector sqrt, rsqrt, and FMA**
114. **359 — Popcount, CLZ y CTZ / Bit-count intrinsics**
115. **360 — Rotaciones y byte swap / Rotate and byte-swap intrinsics**
116. **361 — Prefetch y pause / Prefetch and pause intrinsics**
117. **362 — Contador de tiempo de CPU / CPU timestamp counter**
118. **363 — Selección y fallback de features de CPU / CPU feature selection and fallback**

## Tanda 10 — Floating-point determinista

119. **364 — Modos `precise`, `fast` y `deterministic` / Floating-point modes**
120. **365 — Política de contracción FMA / FMA contraction policy**
121. **366 — Control de rounding / Rounding control**
122. **367 — Control FTZ y DAZ / FTZ and DAZ control**
123. **368 — Estado floating-point por thread / Per-thread floating-point state**
124. **369 — Matemáticas trascendentales deterministas / Deterministic transcendental math**

## Tanda 11 — Paralelismo para programas NativeSharp

125. **370 — Sincronización nombrada entre procesos / Named cross-process synchronization**
126. **371 — Thread pool nativo / Native thread pool**
127. **372 — Jobs tipados / Typed jobs**
128. **373 — Dependencias y cancelación de jobs / Job dependencies and cancellation**
129. **374 — Colas por worker / Per-worker queues**
130. **375 — Work stealing / Work stealing**
131. **376 — Estado floating-point de workers / Worker floating-point initialization**

## Tanda 12 — Depuración extrema

132. **377 — Visualización de ownership y préstamos / Ownership and borrow debugging**
133. **378 — Visualización de movimientos y destrucción / Move and destruction debugging**
134. **379 — Visualización SIMD y matrices / SIMD and matrix debugging**
135. **380 — Handles, delegates y function pointers / Handle, delegate, and function-pointer debugging**
136. **381 — Depuración de VLAs y placement / VLA and placement debugging**
137. **382 — Stack walking de SEH y callbacks / SEH and callback stack walking**
138. **383 — Stack walking de COM, interfaces y closures / COM, interface, and closure stack walking**
139. **384 — Certificación humana Visual Studio/WinDbg / Visual Studio and WinDbg certification**

## Tanda 13 — `comptime`

140. **385 — Núcleo `comptime/#run` / Compile-time execution core**
141. **386 — Límites y aislamiento de comptime / Compile-time execution sandbox**
142. **387 — Dependencias y cache de comptime / Compile-time dependency caching**
143. **388 — Reproducibilidad de comptime / Reproducible compile-time execution**

## Tanda 14 — Robustez avanzada

144. **389 — Sanitizers nativos / Native sanitizers**
145. **390 — LTO / Link-time optimization**
146. **391 — ThinLTO / Thin link-time optimization**
147. **392 — Enlace incremental / Incremental linking**
148. **393 — Matriz N-1/N de compatibilidad / N-1 and N compatibility matrix**

## Tanda 15 — Arquitectura interna 2.0

149. **394 — Verificadores locales entre etapas / Local stage-boundary verifiers**
150. **395 — Layout y ABI completamente en MIR / Layout and ABI fully represented in MIR**
151. **396 — Ownership y cleanup completamente en MIR / Ownership and cleanup fully represented in MIR**
152. **397 — Excepciones y flujo no local en MIR / Exceptions and nonlocal control flow in MIR**
153. **398 — Genéricos e interfaces consolidados en MIR / Generics and interfaces consolidated in MIR**
154. **399 — Encapsulación del backend LLVM / LLVM backend encapsulation**
155. **400 — Runtime ABI formal y versionado / Formal versioned runtime ABI**
156. **401 — Runtime Win32 como biblioteca independiente / Independent Win32 runtime library**

## Tanda 16 — Cierre 2.0

157. **402 — Ledger de breaking changes / Breaking-change ledger**
158. **403 — Migración 1.0.1 a 2.0 / Migration from 1.0.1 to 2.0**
159. **404 — Congelamiento 2.0 RC / 2.0 RC freeze**
160. **405 — Certificación N-1/N final / Final N-1 and N certification**
161. **406 — Baseline completo final / Final complete baseline**
162. **407 — Promoción oficial a NativeSharp 2.0 / Official NativeSharp 2.0 promotion**

## Fuera de alcance deliberado

NativeSharp 2.0 no pretende incorporar por compatibilidad superficial todas las características de
C#. Quedan fuera de este plan `params`, argumentos nombrados, métodos de extensión, interpolación,
`??`, `??=`, `?.`, LINQ, reflexión administrada, garbage collector, `async/await` administrado y un
sistema de tipos COM. Las capacidades incluidas se justifican por programación nativa, control de
memoria, interoperabilidad, rendimiento, depuración o robustez del toolchain.
