# Correccion de regresiones O0 posteriores a method bodies

## SCOPE LOCK

### Enmienda funcional autorizada el 6 de octubre de 2026

Esta enmienda reemplaza exclusivamente las reglas anteriores que conservaban
ediciones de lenguaje, exigencias globales de `unsafe`, movimiento obligatorio,
excepciones verificadas o validacion integral externa. El resto del historial de
este documento se conserva como evidencia de la regresion que condujo a la
decision actual.

- Objetivo autorizado: convertir NativeSharp en un unico lenguaje acumulativo,
  sin semantica seleccionada por `<Language>`, sin edicion publicada en NSMETA y
  sin obligacion de usar `unsafe`, `move`, `copy`, `throws`, `try` o `catch`.
- Naturaleza: cambio funcional deliberado del contrato interno previo a la
  primera publicacion, seguido de correccion integral de regresiones.
- Sintaxis nueva autorizada: `static_cast<T>(value)`,
  `dynamic_cast<T>(value)` y `reinterpret_cast<T>(value)` con semanticas
  independientes y diagnosticos propios.
- Compatibilidad de proyectos: los proyectos nuevos no requieren ni emiten
  `<Language>`; un campo heredado, si existe, se acepta y se ignora sin alterar
  sintaxis, semantica, metadata, ABI ni diagnosticos.
- Compatibilidad de metadata: NSMETA puede conservar una version estructural
  interna para seleccionar su decodificador, pero no publica ni consume una
  edicion del lenguaje y nunca rechaza una fuente por esa etiqueta.
- Semantica de ausencia: un programa que no use casts explicitos, ownership
  optativo ni excepciones conserva la semantica historica de valores copiables,
  llamadas ordinarias y acceso nativo directo.
- Archivos permitidos: productores del compilador bajo `include/` y `src/`,
  infraestructura y pruebas bajo `tests/` y `cmake/` que afirmen directamente
  el contrato retirado, y documentacion de la serie bajo
  `docs/compiler-optimization/`.
- Pruebas que pueden modificarse: unicamente pruebas creadas para exigir una
  edicion, `unsafe`, movimiento/copia forzados o propagacion obligatoria de
  `throws`. Su reemplazo debe comprobar la nueva convencion, no relajar una
  regresion no relacionada.
- Superficies protegidas: todas las demas pruebas historicas, fixtures,
  resultados, ABI, backend no relacionado, runtime, bibliotecas, bindings,
  Saru, instalaciones y repositorios fuera de `A:\nsc-devel`.
- Acciones prohibidas: tocar o instalar en `C:`, ejecutar baseline o
  minibaseline, ejecutar O1/O2/O3, adaptar consumidores externos, usar agentes
  paralelos, introducir recursión o implementar features ajenos.
- Validacion obligatoria: despues de cada cambio coherente se reconstruye desde
  fuentes en `A:` y se ejecuta la bateria CTest completa en O0, usando NSLink
  para los artefactos NativeSharp generados. Una comprobacion puntual no cierra
  una tanda.
- Commits: cada unidad logica se entrega en un commit pequeno con Markdown
  editorial propio; el archivo ajeno `_tmp_strip_saru_exceptions.py` permanece
  sin tocar ni versionar.
- Criterio de cierre: cero ramas operativas por edicion, cero exigencias de
  `unsafe`, casts diferenciados implementados, valores ordinarios no consumidos
  implicitamente, excepciones optativas, proyectos y NSMETA sin contrato de
  edicion y bateria completa O0 en cero fallos.

- Objetivo autorizado: auditar las 234 manifestaciones del reporte posterior a
  la reingenieria semantica, agruparlas por productor comun y corregirlas hacia
  adelante sin retirar las mejoras de rendimiento.
- Naturaleza: correccion de regresiones y compatibilidad del compilador; no es
  una migracion de pruebas, proyectos, Core ni consumidores.
- Oraculo: la bateria historica que pasaba antes de la reingenieria permanece
  byte por byte intacta y conserva su comportamiento observable.
- Revision correcta conocida: el ultimo estado certificado anterior al rango
  `45dea40ec..72f7ad7cb`; el rango de rendimiento se conserva y se corrige sobre
  `development`.
- Archivos permitidos: productores del compilador dentro de `include/`, `src/`
  y nueva cobertura correctiva en archivos nuevos bajo `tests/`; documentos de
  esta serie bajo `docs/compiler-optimization/`.
- Archivos protegidos: pruebas historicas, fixtures, fuentes de prueba,
  expectativas, proyectos `.nsproj`, bibliotecas, runtime, ejemplos y todo
  repositorio o instalacion fuera de `A:\nsc-devel`.
- Pruebas protegidas: las 678 pruebas descubiertas por el arbol CTest actual y,
  en particular, las 234 manifestaciones del reporte externo; ninguna se
  modifica, omite, filtra ni reclasifica para hacerla pasar.
- Acciones prohibidas: tocar o instalar en `C:`, ejecutar baseline,
  minibaseline, matrices O1/O2/O3, adaptar consumidores o incorporar features
  aplazados como `throws` en constructores.
- Cierre: cada causa raiz queda corregida en su productor, con commit pequeno,
  Markdown propio y comprobacion dirigida O0; la certificacion integral queda
  reservada a la ejecucion externa del usuario.

- Se corrige hacia adelante desde `development`; no se restaura un milestone ni se
  retiran las mejoras de rendimiento ya implementadas.
- Ninguna prueba, fixture, proyecto historico, expectativa ni cadena esperada se
  modifica para conseguir un resultado verde.
- No se modifica ni se instala nada en `C:`.
- La validacion de esta serie usa exclusivamente O0. No se ejecutan baseline,
  minibaselines ni matrices O1/O2/O3.
- Cada correccion logica lleva un commit pequeno y un Markdown propio.
- Implementar o extender `throws` no forma parte de esta serie. Al retirar la
  primera barrera de edicion aparecieron cuatro pruebas historicas con `NS3071`
  y una con rechazo de patron `catch`: se corregira la imposicion global que
  altero programas antes validos, sin implementar `throws` en constructores ni
  modificar esos programas.

## Evidencia del reporte externo

El reporte `20261006-181751-ba485e23` contiene 234 fallos, pero no 234 causas
independientes. El analisis estatico los agrupa asi:

| Grupo | Evidencia | Causa primaria |
|---|---|---|
| Edicion del lenguaje | 361 `NS3001` | La omision se confundia con Language 2 explicito |
| Sintaxis de valores | 34 `NS3002` | Fallo derivado de la edicion implicita |
| Superficie de Core | Simbolos y overloads ausentes | Auditar despues de retirar la barrera de edicion |
| Compatibilidad de metadata | Stage 48.2, 50.9, 63, 66 e Item 407 | Validar capacidades, no etiquetas |
| Self-host derivado | Cuatro ausencias de SH41/nsc2 | SH41 no produjo sus artefactos |

## Segundo corte causal despues de la tanda 2

La ejecucion diagnostica del arbol CTest construido encontro 104 pruebas
fallidas. No representa una nueva bateria autorizada ni una certificacion: se
usa exclusivamente como evidencia estatica para evitar corregir sintomas uno
por uno. La agrupacion completa es:

| Productor compartido | Pruebas | Disposicion |
|---|---:|---|
| Entorno CTest sin bibliotecas A-locales | 42 | Infraestructura invalida; no copiar desde `C:` |
| Politicas implicitas de compatibilidad | 17 | Restaurar optatividad; conservar Language 3 estricto |
| Sintaxis Language 3 en proyectos sin etiqueta | 3 | Separar reconocimiento actual de seleccion explicita de politica |
| Cierre NSMETA que aun trata la omision como Language 2 explicito | 1 | Tanda 3 |
| Compatibilidad NSMETA basada en etiquetas | 3 directas y 4 derivadas | Tanda 5 |
| Superficie publica de Core | 2 | Tanda 4; auditar sin modificar Core durante la optimizacion |
| Self-host derivado de productores anteriores | 2 directas y varias transitivas | Tanda 7 |
| Inventario, equivalencia semantica y conducta generada | 3 | Tanda 6, despues de retirar barreras anteriores |
| Diagnosticos agregados aun no atribuibles | 31 | Reabrir con artefactos A-locales validos |

La cifra de cada grupo es una cuenta de pruebas, no de defectos. Las pruebas
que dependen de bibliotecas ausentes no se utilizan para inferir semantica hasta
que el entorno A-only publique sus productores correctos.

## Causa raiz confirmada de la primera barrera

`ProjectLoader` conserva correctamente Language 2 como valor predeterminado,
pero `Project` almacena solamente el valor efectivo. Al perderse la procedencia,
el compilador no puede distinguir entre:

1. un proyecto que declaro expresamente `<Language>2</Language>`, y
2. un proyecto historico que no declaro ninguna edicion.

`ProjectGraphLoader` y `ProjectGraph::addPrecompiledProjects` comparan despues
los valores efectivos y rechazan ambos casos por igual. Eso convierte una
omision compatible en un conflicto explicito y detiene el analisis antes del
parser, la semantica o el backend.

La correccion pertenece al compilador, no a los proyectos historicos. `Project`
debe conservar si la edicion fue declarada. Un componente conectado que no la
declaro adopta la unica edicion explicita exigida por sus dependencias fuente o
precompiladas. Dos ediciones declaradas y contradictorias siguen produciendo
`NS3001`.

## Invariantes de aceptacion

- Un proyecto aislado sin `<Language>` continua usando Language 2.
- Un grafo sin ediciones explicitas continua usando Language 2.
- Un consumidor implicito de un unico contrato Language 3 adopta Language 3.
- La inferencia se aplica al grafo completo, no solo al borde que fallo primero.
- Language 2 explicito frente a Language 3 explicito sigue rechazado con
  `NS3001`.
- Un propietario implicito puede adoptar la edicion unica publicada por NSMETA.
- Un propietario explicito incompatible con NSMETA sigue rechazado con
  `NS3001`.
- Toda reconstruccion de `Project` conserva edicion, procedencia, paquete,
  amigos y politica de panico.
- Ningun archivo existente bajo `tests/`, ningun fixture y ningun `.nsproj` se
  modifica.

## Tandas correctivas

| Tanda | Alcance | Cierre requerido |
|---:|---|---|
| 1 | Diagnostico y bloqueo de alcance | Causa raiz documentada sin proponer cambios a pruebas |
| 2 | Procedencia e inferencia de edicion en grafos fuente | Compilacion limpia y bateria O0 completa sin fallos |
| 3 | Inferencia de edicion para cierres NSMETA | Compilacion limpia y bateria O0 completa sin fallos |
| 4 | Superficie publica de Core | Compilacion limpia y bateria O0 completa sin fallos |
| 5 | Compatibilidad de metadata basada en capacidades | Compilacion limpia y bateria O0 completa sin fallos |
| 6 | Fallos semanticos/backend restantes | Compilacion limpia y bateria O0 completa sin fallos |
| 7 | Cierre self-host derivado | Compilacion limpia y bateria O0 completa sin fallos |
| 8 | Auditoria final y documentacion | Cero fallos O0; baseline reservado para ejecucion externa |

## Estado

- Tanda 1 terminada por analisis estatico.
- Tanda 2 terminada: procedencia e inferencia entre proyectos fuente.
- Tanda 3 terminada: inferencia del cierre NSMETA, con tres pruebas dirigidas
  completas y cinco de seis checks de T33. Su ejecucion final queda no
  verificable en el sandbox porque `cmd.exe` recibe `PATH` vacio.
- Tanda 4 clasificada y cerrada dentro de este alcance: las superficies de Core
  faltantes fueron eliminadas por el commit independiente `9232bcfc0`; su
  restauracion o migracion corresponde a bibliotecas y no se encubre mediante
  cambios al compilador ni a las pruebas historicas.
- Tanda 5 terminada: etiquetas informativas, contenido NSMETA realmente
  decodificado, capacidades requeridas por una instalacion y productores legacy
  conocidos vuelven a ser contratos separados. Los seis tests dirigidos pasan
  juntos sin modificar consumidores historicos.
- Tanda 6 terminada: ocho contratos unitarios de las rutas semanticas
  reingenierizadas y el caso de conducta T37 pasan de forma dirigida en O0. Los
  `error*.log` se confirmaron como artefactos de invocaciones negativas y no se
  usaron como sustituto del resultado CTest.
- Tanda 7 terminada: la edicion efectiva del grafo ya no opta silenciosamente
  las fuentes implicitas a reglas Language 3. Cuatro contratos dirigidos pasan;
  el self-host O0 se emite y su propia imagen valida grafo, modulos y las 27
  unidades raiz sin errores.
- Tanda 8 terminada: la serie conserva las pruebas y proyectos historicos,
  contiene un Markdown por tanda y no deja cambios propios sin commit. El
  alcance del compilador queda cerrado con evidencia dirigida O0; la
  certificacion completa permanece externa.
- No se ejecuto baseline ni minibaseline.
