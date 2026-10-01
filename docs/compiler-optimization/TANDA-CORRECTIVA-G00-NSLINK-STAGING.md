# Tanda correctiva G00 — NSLink local dentro del staging

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

La corrida `20261001-082838-4569e779` construyó e instaló correctamente `nsc` y las bibliotecas
bootstrap en `A:\native-sharp-3.0-baseline`. Las pruebas self-host que sólo necesitaban al
compilador avanzaron, pero SH27, SH29, SH30, SH31, SH32, SH33, SH34, SH35, SH36 y SH38 fallaron
al intentar crear import libraries o enlazar con NSLink.

El fallo no estaba en esos diez subsistemas. CMake había configurado correctamente las pruebas
para ejecutar `A:\native-sharp-3.0-baseline\bin\nslink.exe`, pero el constructor nunca colocaba
ese archivo en el staging. Al mismo tiempo, el plan y el bootstrap lock todavía tomaban NSLink de
`C:\native-sharp`, mezclando el producto 2.0 instalado con el candidato 3.0.

## Regla implementada

- El origen predeterminado de NSLink es ahora la construcción local más reciente:
  `A:\nslink-devel\build-debug\bin\nslink.exe`.
- Antes de gastar CPU en la construcción, el Gate 0 rechaza un NSLink cuyo origen no esté en la
  unidad `A:` o que se encuentre dentro del propio staging que va a reiniciar.
- Después de instalar `nsc`, el runner copia NSLink en
  `A:\native-sharp-3.0-baseline\bin\nslink.exe` y compara SHA-256 entre origen y copia.
- Las pruebas, `NSC_LLD_LINK`, el bootstrap lock y las mediciones ABI usan exclusivamente esa
  copia del staging durante un baseline.
- `C:\native-sharp` deja de ser origen para el Gate 0. Continúa siendo únicamente el destino de
  promoción posterior a una certificación verde.
- La configuración guardada de la GUI migra automáticamente el valor heredado exacto
  `C:\native-sharp\bin\nslink.exe` al nuevo origen en `A:`.
- La barra de progreso cuenta la nueva etapa explícita `Preparar NSLink del baseline`.

## Archivos modificados

- `nscbuilder.py`: selecciona, valida, copia, verifica y usa NSLink desde `A:` dentro del staging;
  actualiza el bootstrap lock, la medición ABI, la GUI, la consola y el conteo de etapas.
- `tests/selfhost/test_bootstrap_infrastructure.py`: congela el origen predeterminado, comprueba la
  copia y su uso efectivo, y prueba el rechazo de un origen en `C:`.

## Mejora obtenida

El baseline vuelve a ser autocontenido: `nsc.exe`, `nslink.exe` y las bibliotecas bootstrap que
ejercita CTest viven bajo una sola raíz candidata en `A:`. Esto elimina la mezcla silenciosa entre
2.0 y 3.0, evita los fallos en cascada por ejecutable ausente y hace que el hash certificado de
NSLink corresponda a la herramienta realmente usada.

## Validación específica

- Sintaxis de `nscbuilder.py` y de su módulo de pruebas: correcta.
- Cuatro pruebas unitarias dirigidas de infraestructura: 4/4 correctas en 0.028 s.
- Plan predeterminado: origen `A:\nslink-devel\build-debug\bin\nslink.exe`, destino efectivo
  `A:\native-sharp-3.0-baseline\bin\nslink.exe`, cero problemas de validación.
- Arranque directo del NSLink seleccionado: `NSLink 0.1.0 -- Win64/x64 linker for the NativeSharp
  ecosystem`, exit 0.
- No se ejecutó CTest ni baseline.

## Resultado esperado en la siguiente corrida

SH27–SH38 ya no deben fallar por ausencia de `nslink.exe`. Esta tanda sólo corrige la
infraestructura y la procedencia de la herramienta; cualquier fallo semántico que aparezca después
de que NSLink sí se ejecute deberá evaluarse por separado con su evidencia real.
