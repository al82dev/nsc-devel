# NS304-T01-F05 — fallo terminal de inicialización estática

## Contrato

Una inicialización estática que termina normalmente publica `initialized`. Si
una salida excepcional abandona sus inicializadores de campo o su constructor
estático explícito, el coordinador publica `failed` antes de continuar el mismo
unwind. `failed` es terminal: ninguna lectura posterior vuelve a reclamar el
guard ni reintenta el cuerpo.

El acceso posterior a un tipo fallido termina mediante el `PanicProfile` del
proyecto. `abort` usa el trap de panic del proceso; `unwind-native` eleva el
código SEH reservado y conserva la frontera nativa. Ningún perfil cruza FFI.

## Implementación

- `StaticInitializationState` publica qué estados son terminales y cuál es el
  único estado desde el que puede iniciarse un intento.
- `ProjectOopLlvmLowerer` usa los ordinales canónicos del enum, no constantes
  independientes del backend.
- El guard generado posee personality Win64 y un `cleanuppad` propio. Las
  llamadas potencialmente divergentes del inicializador usan `invoke`; el pad
  publica `failed=3` con orden secuencialmente consistente y propaga el unwind
  original al llamador.
- Fast path, resultado perdido del CAS y espera entre hilos distinguen
  `initialized=2` de `failed=3`. Sólo `uninitialized=0` puede reclamar
  `initializing=1`; ningún camino desde `failed` vuelve al CAS.
- Una consulta posterior de `failed` reproduce panic según el perfil del
  proceso y usa trap si `RaiseException` retorna de forma imposible.

## Evidencia puntual

- `TL-LANG-298`: `PASS`, 427 microsegundos; estados terminales, prohibición de
  retry y contratos `abort`/`unwind-native` exactos.
- Guards: `TL-LANG-297` `PASS` en 71138 microsegundos, `TL-LANG-294` `PASS` en
  7 microsegundos y `TL-LANG-160` `PASS` en 4239 microsegundos.
- `nsc_stage37_static_initialization_tests`: `PASS`, 67 checks. Verifica
  `invoke -> cctor.failure_unwind -> store atomic i8 3 -> cleanupret`, el
  camino de reconsulta `failed`, ambos perfiles, LLVM verifier, O2 y COFF.
- Productores focales: objeto `ProjectOopLlvmLowerer.cpp.obj`, archivo
  `nsc_llvm.lib` con sus 18 objetos declarados, Stage37 y
  `nsc_component_tests`; no se regeneró configuración ni se ejecutó batería.

## Gate F05

```text
success = publish initialized(2)
exception = publish failed(3) -> preserve original unwind
waiter = spin while initializing(1), then initialized or failed
failed reaccess = panic(process profile)
retry from failed = forbidden
```
