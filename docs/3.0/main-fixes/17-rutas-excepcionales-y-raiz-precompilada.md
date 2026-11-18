# Rutas excepcionales, constructores que lanzan y raíz precompilada (M1-07)

Rama `development-main-fixes`. Este documento cierra estos pendientes reales de la mejora 1:

- el constructor con `throws` del local;
- los locales en callables que lanzan;
- try/catch/finally;
- los proyectos que usan `NativeSharp.Core`.

Evidencia en `A:\nsc-main-fixes-evidencia\claude`:

- `m1-07` (a–k);
- `m1-core` (a–c);
- `logs\p10-*` a `logs\p17-*`;
- `logs\libs-out-p12.log`;
- `logs\libs-out-p17.log`.

## Qué cambia

1. **Constructor que lanza.** Un constructor que declara un tipo de error puede contener, además de las
   sentencias ya demostradas:
   - `throw new X(args)`;
   - `if (cond) { throw new X(args); }`.

   Condiciones:
   - los argumentos son escalares cerrados (doc 16);
   - la condición es un árbol acotado de comparaciones y operaciones lógicas o aritméticas sobre escalares
     cerrados;
   - el objeto de la excepción nunca recibe `this`.

   Si la construcción falla, no se ejecuta el destructor del local. Los inicializadores de campo se ejecutan
   después del cuerpo, así que tampoco se crea el hijo owned.
2. **Callables que lanzan.** Se quita la regla anterior que, en un callable con tipo de error, prohibía
   toda llamada y toda creación. La limpieza del objeto de marco se ejecuta en cada salida excepcional:
   pad de limpieza con `local.addr`, nulo hasta construir.
   - Ahora se acepta una llamada que lanza sobre el propio local, y el local también puede pasarse como
     argumento a un callee que lanza.
   - Sigue rechazada una excepción que lleva `this` (`throw new E(this)`): el resumen de escape la ve.
3. **try/catch/finally y throw.** Se sustituye la lista cerrada `safeFinallyCallable`: ahora son flujo
   ordinario. Sigue rechazado un local declarado dentro de un `finally`: un funclet de limpieza SEH no puede
   contener acciones excepcionales, y el mismo límite de LLVM afecta a los locales de heap ahí.
4. **Cuerpos de constructor y destructor.**
   - **Constructor:** se acepta `field = valor` con receptor implícito.
   - **Destructor:** puede tener varias escrituras estáticas acotadas.
   - **Escrituras estáticas:** admiten `*` y la lectura de campos escalares propios.
5. **Raíz precompilada protegida (`NativeSharp.Core.Object`).** Una cadena implícita por omisión que
   termina en una raíz precompilada protegida queda demostrada. La evaluación de receptor de esa
   construcción es `NoEscape`. La raíz debe cumplir:
   - no tiene campos ni base;
   - tiene contratos de receptor protegidos (`.nsreceiver` con su `.lock`);
   - se entra por su constructor precompilado sin parámetros.

   Antes, en todo proyecto con Core un `Box b;` sin constructor explícito daba «construction or destruction
   is not proven safe».

## Ejecución (O0 y O2; en el IR, los locales son `%localN.object = alloca`)

| Sonda | Contenido | Resultado |
| --- | --- | --- |
| g-runtime | constructor que lanza, desenrollado tras una llamada, throw antes de la declaración, anidados, llamada que lanza sobre el local | PASS (igual que la línea base de heap h-runtime-heap) |
| i-try-runtime | local dentro de try, alrededor de try, en catch, con finally, try anidados; se comprueba el orden de destructores | PASS (igual que j-try-heap) |
| k-shapes k1–k5 | finally con constructor estático, llamada en finally (local externo e interno), solo catch, throw con argumento | PASS |
| b-implicit-throws / e-implicit-unwind | con `NativeSharp.Core` real (Exception de biblioteca) | PASS |
| m1-core b, c | Core: constructor explícito, implícito con hijo owned, virtual + upcast | PASS |
| m1-core a | Core copiado sin `.nsreceiver` | rechazado (falla cerrado, correcto) |

## Hallazgo ajeno a estas mejoras

`k6-rethrow` (`catch (Boom e) { throw e; }`) falla en O2:

- con un local de heap da una violación de acceso (`k-heap\k6-rethrow`);
- con un objeto de marco se observa una cuenta de destructores incorrecta.

En O0 pasan los dos. Es un defecto previo de la generación de excepciones al relanzar, independiente del
almacenamiento del local. Queda reportado, no corregido.

## Pruebas

- `nsc.class-value-origins`.
  - Positivos nuevos: `LocalThrowingConstructor`, `LocalInThrowingCallerWithCalls`,
    `LocalThrowingCallOnLocal` y `LocalImplicitFieldConstructorAndDestructorStatements`.
  - Controles anteriores que ahora son positivos: `LocalExceptionalUnknownCall`, `LocalFinally`,
    `LocalFinallyStaticInitializer`, `LocalFinallyCast`, `LocalCatch` y `LocalCatchExceptionArgument`.
  - Negativos: `LocalThrowingConstructorPublishesThis`, `LocalThrowingConstructorCallCondition`,
    `LocalThrowingCallExceptionCarriesThis` y `LocalDeclaredInFinally`.
- Pasan también:
  - t06;
  - stage50_7;
  - stage56 c7;
  - m1-run (O0/O2).
- Las rutas y sondas no cambian.
- Bibliotecas: 32/32.
