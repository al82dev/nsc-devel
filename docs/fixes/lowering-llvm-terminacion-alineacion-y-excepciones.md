# Lowering LLVM: terminación, alineación, excepciones y frames

Fichas LKF-436, 439, 440, 443, 445 a 449, 452, 457, 460, 464, 468, 469, 471 a 473, 476, 477, 483,
491, 493, 495, 498, 499 y NSC-NEW-029. Cada ficha se contrastó con programas que se compilan y
ejecutan en O0 y O2, con el IR LLVM emitido o, si no hay entrada desde fuente, con la lectura del
código y sus invariantes.

## Defectos corregidos

- **LKF-440.** Un argumento que nunca termina (`b.F(loop { throw new Exception(); })`) cierra el
  bloque. La llamada directa ya se detenía ahí. La llamada virtual y la de interfaz cargaban el
  destino detrás del terminador y pedían el tipo ABI de `never` ("cannot map type key to LLVM type:
  builtin:never"). Un constructor y un inicializador de arreglo emitían instrucciones tras el
  terminador ("Terminator found in the middle of a basic block"). Las cuatro rutas se detienen ahora,
  como la ordinaria. El delegado y el evento tienen la misma comprobación.
- **LKF-436 / LKF-445.** Un campo de un struct `pack(1)` se leía y escribía con la alineación del
  tipo: `store i64 40, ... align 4` en el offset 1. El inicializador de agregado hacía lo mismo. En
  x64 un mov escalar tolera el desalineamiento, pero LLVM puede fusionar accesos alineados (SSE) y es
  comportamiento indefinido en el IR. La carga, la escritura, la asignación compuesta y el
  inicializador toman ahora la alineación real del lugar: la del campo, reducida por cada struct que
  lo contiene, o la que garantizan la ranura y el offset. Nunca se sube una alineación. Un elemento
  `volatile` de un inicializador se escribe como volatile, como en la ruta de bitfields.
- **LKF-446.** Si ninguna cláusula atrapaba la excepción, se ejecutaban todas las limpiezas de la
  función y se relanzaba con una llamada simple. Así salía de la función aunque un `try` contenedor
  de la misma función tuviera el `catch` adecuado: `try { try { throw new BErr(); } catch (AErr a)
  { } } catch (BErr b) { return 42; }` devolvía el control al llamador (salida 3 en lugar de 42).
  Con un manejador contenedor en el mismo funclet, ahora solo corren las limpiezas dentro de su `try`
  (un `finally` interno, los dueños declarados dentro) y el relanzamiento se desenrolla hacia él. Los
  dueños declarados fuera del `try` externo siguen vivos.
- **LKF-447 / LKF-476.** El asignador del runtime devuelve null si el heap se agota. El frame de
  corrutina y el de generador se guardaban y se llenaban sin comprobarlo. Un frame nulo atrapa
  ahora con `AllocationFailure`, como las demás reservas.
- **LKF-448.** La espera del inicializador de otro hilo en un local `static` es un bucle de cargas
  atómicas. El algoritmo es correcto: un CAS elige al único inicializador, los demás esperan el
  estado 2 o 3, y la recursión del mismo hilo no espera. El bucle emite ahora una pausa
  (`llvm.x86.sse2.pause`), como las guardas de MSVC y libc++.
- **LKF-449.** Un inicializador de `using` que nunca termina guardaba el recurso tras el
  terminador. Ahora no hay recurso que guardar, cuerpo que ejecutar ni limpieza que activar.
- **LKF-457.** El análisis semántico, HIR, MIR y el lowering LLVM recorren en algunos puntos el
  árbol de una expresión de forma recursiva. Una expresión generada de 5000 términos desbordaba la
  pila de 1 MiB de los workers en LLVM, y una de 20000 en MIR (0xC00000FD). Los workers son
  `std::thread` y toman la reserva del ejecutable. `nsc` reserva ahora 128 MiB por hilo (`/STACK`).
  Solo se confirman las páginas que toca un árbol profundo. Expresiones de 100000 y 300000 términos,
  y 20000 paréntesis anidados, compilan y ejecutan.
- **LKF-460.** El ordinal de un placeholder de ensamblador en línea se acumulaba sin límite, en el
  parser y en LLVM. `$18446744073709551616` daba la vuelta a `$0` y se aceptaba. La acumulación se
  satura al pasar la cantidad de operandos, y el parser lo rechaza con su diagnóstico.
- **LKF-471.** La señal determinista de error de un await bloqueante es `-1` para un resultado
  entero. Para un `double`, un `string` o un struct se emitía una constante entera y el verificador
  rechazaba el phi, así que `double d = await F();` con `async double F()` no compilaba. Esos
  resultados toman su valor cero.
- **LKF-473.** La glue de destrucción de un iterador llamaba a su función Drop sin comprobarla. Un
  local `Iterator<int, int> it;` declarado y nunca asignado vale cero, y su salida de scope llamaba a
  la dirección cero (0xC0000005). Como un future, un iterador en cero no destruye nada. La parte de
  enlace dinámico de la ficha es interoperabilidad y queda fuera de alcance.
- **LKF-483.** El poll de una llamada esperada era el del miembro elegido estáticamente. Esperar una
  llamada async virtual o de interfaz ejecutaba el cuerpo base sobre el future de la redefinición:
  `B b = c; await b.G()` devolvía el valor de `B.G`. Una llamada con despacho dinámico se espera
  ahora como un valor future, con el poll que trae el future. La comprobación semántica del await
  síncrono sigue rechazando un productor conocido que puede suspender. Un productor detrás de un
  despacho dinámico no se conoce en compilación: si suspende, el await síncrono entra en pánico
  (salida 142) en lugar de continuar con un resultado sin calcular.
- **LKF-498.** Una limpieza de expresión completa que fallaba empujaba `nullptr` y además el valor.
  La pila de la máquina de expresiones quedaba con una entrada de más y el frame padre emparejaba sus
  operandos con valores equivocados. Un fallo produce ahora exactamente un `nullptr`. No hay entrada
  desde fuente.
- **NSC-NEW-029.** Un método async o generador de instancia no podía usar `this`. El poll y el
  MoveNext son estáticos y no tenían receptor: `public async int G() { return V; }` fallaba con
  "lowering failed". El constructor guarda ahora el receptor de clase en el frame, en una ranura
  propia de `asyncFrameParameters`, y el poll y el MoveNext lo leen de ahí. Un receptor struct es
  una dirección prestada que el frame podría sobrevivir. No se captura, y su uso se rechaza con un
  mensaje que pide pasar los valores como parámetros.

## Fronteras verificadas

- **LKF-436 (ruta procedural), LKF-491, LKF-493, LKF-495, LKF-499.** `LlvmLowerer`
  (`ImplementationStatements.cpp`), `MirDumper`, `SsaDumper`, `OptimizationDumper` y `MirVerifier`
  pertenecen a la ruta procedural MIR/SSA. El driver solo incluye sus cabeceras y no los llama;
  `nsc` compila por la ruta OOP. Esa ruta procedural solo la usan pruebas unitarias. La corrección
  de alineación se aplicó en la ruta OOP.
- **LKF-439 / LKF-443.** `allocateNativeString` limita la longitud a `0x7fffffff`, que es el
  contrato público (`Length` es `int`). Dos cadenas que cumplen ese límite suman como mucho 2^32, que
  no desborda i64, y la concatenación condicional reserva por la misma función, que comprueba el
  total.
- **LKF-452.** Un destructor no puede declarar `throws` ni dejar escapar la excepción de una llamada,
  así que los wrappers `nounwind` de destructor y `Deallocate` no ocultan un desenrollado posible.
- **LKF-464.** La condición del sufijo `*` en la lectura de un parámetro capturado nunca se cumple
  tras la comparación de tipos, pero la lectura de un parámetro `ref` capturado se resuelve por otra
  ruta: un generador y un async con `ref int` leen y escriben el original.
- **LKF-468.** El offset cero de `futureFieldOffset` es un centinela: el campo 0 es el estado y
  ningún otro campo vive ahí. Todo future tiene layout con poll (miembro 3) y Drop (miembro 5), el
  importador falla si no puede construirlo, y el campo de error (4) solo se lee en futures con error.
- **LKF-469.** Un inicializador estático no puede lanzar (las excepciones son comprobadas), así que
  la ruta de fallo solo la alcanza un pánico con perfil unwind, que publica el estado fallido.
- **LKF-472.** La traza de reservas es una ayuda de diagnóstico con mensajes fijos; no cambia la
  ejecución.
- **LKF-477.** Incluir las referencias de `Dispose` en las aristas sobreaproxima la alcanzabilidad:
  puede conservar código de más, nunca omitirlo.

## Pruebas

`nsc.compiler.llvm-lowering-boundaries`: casos en O0/O2 y comprobaciones del IR, cada rechazo con
su control. Con el compilador anterior:

- LKF-440: las llamadas virtual y de interfaz, el constructor y el inicializador no compilaban.
- LKF-445: `align 4` en el offset 1 de un struct `pack(1)`.
- LKF-446: los anidamientos devolvían 3 (la excepción salía de la función).
- LKF-457: 0xC00000FD desde 5000 términos.
- LKF-460: el placeholder de veinte dígitos se aceptaba.
- LKF-471: el await bloqueante de double, string y struct no compilaba.
- LKF-473: 0xC0000005 al salir del scope.
- LKF-483: el await virtual devolvía 1.
- NSC-NEW-029: "lowering failed".

Las pruebas `stage35.recovery`, `stage36.recovery` y `stage35-recovery.mir-dump` estaban
desactualizadas desde `9f6dff63d` (un local inicializado con `new` es dueño RAII y no se borra
explícitamente) y C-01 (un `new` no llega a un retorno ni a un parámetro sin `owned`). Sus fixtures
borran ahora parámetros o usan un retorno `owned`. El caso negativo `InterfaceMethodBody` pasó a ser
positivo porque un cuerpo en un método de interfaz es su implementación predeterminada
(NS304-T02-F01).
