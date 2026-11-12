# Consulta estática de campos: registro de pruebas dirigidas

Última matriz binaria versionable, con base genérica de otro módulo: salida 0,
112 casos y 487 operaciones registradas; nsc SHA-256 estable
`4d37755ca759d43f54f370c7382caebf2762026756a6ba84d9543e103a4fffaa`.
Comando `A:/nsc-limitaciones-campos-evidencia/static-field-binary-security-current.cmd`;
evidencia `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-sk0f4v_t/results.json`.
Incluye productores y controles, no representa 112 defectos. Los intentos de
base internal o sin referencia fallaron al parsear el productor y no probaron
la guarda Fields (`generic-external-base-visibility-20261112T181235472306Z`).

Última tanda binaria, posterior a la implementación de `Box<T> : Base<T>`:
`A:/nsc-limitaciones-campos-evidencia/static-field-binary-security-current.cmd`,
salida 0, 103 casos y 448 operaciones registradas sin fallos; incluye
productores, controles positivos y negativos, O0/O2 y revisión de imports.
El nsc mantuvo SHA-256
`a38670ed0b21ff8a12fe96958938e0f9dafc324b6b906fd2bdee5ee983626663`.
Evidencia: `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-86r7wedn/results.json`.
La sonda artificial de certificado Base<T> rechazó ausencia, duplicado,
identidad, módulo y fuente alterados; control aceptado:
`A:/nsc-limitaciones-campos-evidencia/generic-base-certificates-20261112T180216404533Z/results.json`.
No es una prueba de alteración de metadata en disco.

El compilador de ambas tandas fue `A:/nsc-limitaciones-campos-evidencia/build/bin/nsc.exe`,
SHA-256 `099ea1371826e4fe75006d40870f6272f3081b2c8827f55506f105668716643a`;
cada runner comprobó que no cambió durante su ejecución. La compilación es
incremental y las pruebas se dirigen únicamente a estas capacidades; no son
una batería completa ni certifican todo el lenguaje.

- Fuente: `A:/nsc-limitaciones-campos-evidencia/static-field-source-final-current.cmd`,
  salida 0, 26 casos y 136 operaciones registradas, sin fallos. Evidencia:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-wu7fu_f4/results.json`.
- Binario: `A:/nsc-limitaciones-campos-evidencia/static-field-binary-security-current.cmd`,
  salida 0, 91 casos y 407 operaciones registradas, sin fallos. Evidencia:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-mow9rzbq/results.json`.
  Los conteos incluyen productores y controles negativos, no son número de defectos.
- Herencia genérica binaria y receptores: O0/O2, con y sin sidecar, resultados
  `generic-inherited-baseline-20261112T172602667562Z/results.json` y
  `generic-inherited-negative-20261112T172602659470Z/results.json`, bajo la
  carpeta de evidencia anterior. Una sonda de mutación artificial rechazó
  certificado de base ausente/duplicado/ajeno y alteración textual de base:
  `inherited-certificate-probe-20261112T173408109143Z/results.json`. No prueba
  metadata externa maliciosa ni un clon de AST alterado.
- Unitario de introspección con los tres proyectos reales configurados: 64
  comprobaciones, sin SKIP; log
  `A:/nsc-limitaciones-campos-evidencia/unit-inherited-certificate.log`.
  Incluye cinco mutaciones de certificados en memoria. No cubre modificación
  de un clon AST ni de metadata en disco.
- Fixture versionable de herencia con campo oculto y unitario opt-in:
  `A:/nsc-limitaciones-campos-evidencia/unit-inherited-versionable.cmd`, salida 0,
  sin SKIP; evidencia final `temp/nsc-binary-fields-rcub1ew6/results.json` bajo la
  carpeta de evidencia, con hashes del compilador, unitario y productor. El comando
  versionable dirigido es `python -B tests/integration/static_field_binary_fields.py
  <nsc> --inherited-certificate <unitario>`, con `NSC_TEST_TEMP_ROOT` en A: y
  variables de runtime del build aislado. El runner prepara los proyectos y el
  productor desde los fixtures versionados antes de ejecutar el unitario. Conserva
  la limitación anterior del clon AST y no demuestra metadata adulterada en disco.
  El registro CTest propuesto se conserva sin aplicar en
  `A:/nsc-limitaciones-campos-evidencia/register-inherited-certificate.patch`;
  `git apply --check` pasó, pero la prueba no se ejecutó mediante CTest. Requiere
  construir antes el target unitario y configurar compilador, runtime y temporales
  aislados; CTest no construye automáticamente el ejecutable.
- Certificados de plantilla de `Base<T>`: el mismo modo dirigido prepara además
  `GenericBaseProducer.ns` y `GenericBaseConsumer.ns`. Con el compilador SHA-256
  `4d37755ca759d43f54f370c7382caebf2762026756a6ba84d9543e103a4fffaa`, el
  unitario pasó 82 comprobaciones sin SKIP; salida y hashes en
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-jg_85soj/results.json`.
  El control válido y cinco alteraciones de certificado se analizan en memoria;
  no prueban metadata modificada en disco ni mutación del clon AST.
- Base genérica en otro módulo: `generic_external_base.cmd`, productores Alpha,
  Beta y Child en binarios separados; 16 consumidores ordinarios y Fields con y
  sin homónimo, O0/O2 y con/sin sidecar compilaron y ejecutaron correctamente.
  Evidencia `A:/nsc-limitaciones-campos-evidencia/generic-external-base-20261112T181235477648Z/results.json`.
  Una sonda independiente de certificados de base externa acepta el control y
  rechaza cinco mutaciones en memoria, con 783 hashes estables:
  `A:/nsc-limitaciones-campos-evidencia/external-base-certificates-20261112T181323304934Z/results.json`.
  Los casos de base `internal` o referencia omitida se rechazan durante el parseo
  del productor, antes de ejercitar la guarda de Fields; evidencia
  `A:/nsc-limitaciones-campos-evidencia/generic-external-base-visibility-20261112T181235472306Z/results.json`.
- Base nominal binaria en otro módulo: con el mismo compilador `4d37755c...`,
  productores Alpha, Beta y Child separados y ocho controles ordinarios O0/O2
  con/sin homónimo y sidecar pasan; ocho consultas Fields rechazan
  `static field iteration cannot verify this type's complete field declarations`.
  Evidencia `A:/nsc-limitaciones-campos-evidencia/nominal-external-base-20261112T181744829831Z/results.json`.
  Los controles de base `internal` y referencia ausente se rechazan al parsear
  el productor, antes de Fields: `A:/nsc-limitaciones-campos-evidencia/nominal-external-base-visibility-20261112T181819182962Z/results.json`.
- Tras la corrección, el runner dirigido `static-field-binary-security-current.cmd`
  terminó con salida 0 usando el compilador SHA-256
  `7595a2bfa70871976afef19c2d7fca0c902b05b2fc2d622afef586efac76ae09`
  sin cambio de hash durante la ejecución. Incluye base nominal externa, tipos
  homónimos, ejecución con y sin sidecar, cadena entre tres módulos y rechazo de
  receptores incompatibles, con O0 y O2. Comandos, diagnósticos, imports y
  resultados: `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-uhs678me/results.json`.
- En copias sintéticas de metadata, omitir la dependencia hizo rechazar Fields y
  alterar su fingerprint hizo rechazar la preparación; la dependencia duplicada
  fue normalizada por el codec, por lo que esa prueba no acredita una guarda de
  duplicados. Evidencia `A:/nsc-limitaciones-campos-evidencia/synthetic-dependency-20261112T183227931059Z/results.json`.
  En una sonda distinta, duplicar en memoria el certificado de `Alpha.Base` sí
  produjo rechazo; también se rechazaron su ausencia y su identidad alterada:
  `A:/nsc-limitaciones-campos-evidencia/nominal-external-certificates-20261112T183339018828Z/results.json`.
  Esa mutación en memoria no demuestra el comportamiento ante metadata maliciosa
  en disco ni autenticidad externa.

- Herencia genérica en fuente del mismo proyecto: antes del cambio, el control
  ordinario ejecutó O0/O2 y Fields rechazó `cannot verify the generic base`;
  `A:/nsc-limitaciones-campos-evidencia/local-generic-inheritance-20261112T184005345516Z/results.json`.
  El runner fuente versionado terminó con salida 0 tras el cambio, incluyendo
  `Base<T>`/`Box<T>`, campos homónimos y ocultados, receptor base o tipo
  homónimo incompatibles y acceso privado ordinario rechazado en O0/O2.
  Comandos, imports, diagnósticos y hash del compilador se conservan en
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-m9yy19wj/results.json`.
  Este resultado no acredita bases nominales locales ni bases fuente de otro
  proyecto; dichas formas permanecen fuera de esa tanda.
- Base nominal fuente del mismo proyecto: el control ordinario pasó y Fields
  rechazó `cannot verify the generic base identity` antes de la corrección;
  `A:/nsc-limitaciones-campos-evidencia/local-nominal-base-20261112T184526211256Z/results.json`.
  El runner fuente versionado terminó 0 después de la corrección en O0/O2,
  incluidos ocultación, homónimos y rechazos de receptores y acceso privado:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-jkc1t86_/results.json`.
  No acredita una base fuente de otro proyecto.

- Con `ProjectReference` fuente, `Box : Alpha.Base<int>` rechazaba Fields
  antes de seleccionar la jerarquía aunque el control ordinario ejecutaba
  O0/O2: `A:/nsc-limitaciones-campos-evidencia/source-project-base-20261112T185008582470Z/results.json`.
  El runner fuente versionado posterior terminó 0 con base pública, homónimos,
  ocultación y receptores incompatibles, O0/O2:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-cib0tcmj/results.json`.
  El control ordinario de base internal rechaza visibilidad; Fields rechaza
  antes por jerarquía y no prueba esa misma guarda. El primer intento del
  control Beta utilizó un nombre de proyecto inválido y no constituye prueba;
  la repetición válida está en
  `A:/nsc-limitaciones-campos-evidencia/source-project-base-20261112T190356778062Z/results.json`.

- Iteraciones anidadas con descriptores distintos: el control manual ejecutó
  O0/O2 antes del cambio y las consultas anidadas rechazaron explícitamente:
  `A:/nsc-limitaciones-campos-evidencia/nested-static-fields-20261112T190714701083Z/results.json`.
  El runner fuente versionado terminó 0 O0/O2 con dos y tres niveles, campos
  heredados homónimos discriminantes, receptores inválidos y validación de
  descriptor y control de flujo internos con exterior vacío:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-c23ntggy/results.json`.
  Un descriptor interno con el mismo nombre sigue rechazado expresamente; no
  se acredita shadowing. Tampoco se acredita restauración de vínculos C3 ni
  todas las formas de `field.Type` o de control de flujo anidadas.

- Sombra léxica del descriptor: el bucle interior homónimo introduce un nuevo
  descriptor únicamente en su cuerpo; el header conserva el descriptor exterior
  y al salir se restaura el exterior. El control de variables ordinarias pasa y
  la reproducción anterior de Fields rechazaba sombra:
  `A:/nsc-limitaciones-campos-evidencia/shadow-controls-20261112T191723990625Z/results.json`.
  El runner posterior terminó 0 O0/O2 con restauración, `field.Type` int/long,
  header `Fields(field.Type)` y receptor equivocado:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-cq6t6iwi/results.json`.
  La declaración `field.Type` fuera de ámbito se rechaza en parser; no prueba
  una comprobación semántica de escape. En una selección exterior vacía, un
  receptor incompatible en el cuerpo interior no instanciado compiló y ejecutó:
  `A:/nsc-limitaciones-campos-evidencia/shadow-type-controls-20261112T192340817568Z/results.json`.
  La validación sintáctica del cuerpo vacío no demuestra validación semántica de
  receptores dependientes de campos que no se seleccionaron.

- Lambdas en el cuerpo de Fields: el control ordinario ejecutaba O0/O2 y tanto
  la lambda sin descriptor como la que usa `field.Get` se rechazaban antes del
  cambio; `A:/nsc-limitaciones-campos-evidencia/lambda-fields-20261112T192927985886Z/results.json`.
  Tras el cambio, el runner fuente terminó 0 en O0/O2 con parámetro, captura
  explícita del receptor, lambda independiente y rechazo de descriptor aislado:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-5bd5c2vi/results.json`.
  `break` y `continue` dentro de lambda siguen rechazados por esta ruta.
  La repetición con un `break` válido en una lambda ordinaria y su variante
  dentro de Fields pasó las expectativas O0/O2:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-150f_xci/results.json`.
- Hallazgo separado: una closure con `[copy a]` transferida fuera de su bloque
  compila pero tanto el control ordinario como la variante Fields terminan con
  `0xC0000374` en O0/O2. No se atribuye exclusivamente a Fields ni se considera
  validado el lifetime de closures escapadas. Fuentes y resultados:
  `A:/nsc-limitaciones-campos-evidencia/field-lambda-20261112T193510098130Z/results.json`.

- Lambdas genéricas en Fields: la lambda ordinaria ejecutaba O0/O2 y las dos
  variantes Fields se rechazaban antes por sintaxis genérica no admitida:
  `A:/nsc-limitaciones-campos-evidencia/generic-lambda-fields-20261112T193936232149Z/results.json`.
  El runner posterior terminó 0 con parámetros de tipo, `field.Name`,
  `identity<field.Type>` sobre campos int/long y controles negativos de
  receptor, sombra y escape:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-owgfd0eb/results.json`.
  Constraints y parámetros const/pack siguen fuera del subconjunto. La lambda
  genérica `field.Get(value)` con receptor U=A y el control ordinario `value.X`
  rechazan inferencia tanto con retorno inferido como `-> int`; no se atribuye
  exclusivamente a Fields ni se presenta Get genérico válido como probado:
  `A:/nsc-limitaciones-campos-evidencia/generic-field-lambda-20261112T194615983862Z/results.json`.

- Saltos sin etiqueta hacia `static foreach`: antes se rechazaban en O0/O2,
  aunque los controles ordinarios y saltos de bucles interiores ejecutaban:
  `A:/nsc-limitaciones-campos-evidencia/static-flow-controls-20261112T200050509212Z/results.json`.
  La decisión adoptada es que `break` sale de esa iteración y `continue`
  continúa con la siguiente copia, conservando los ámbitos y su limpieza. El
  runner fuente posterior terminó 0 O0/O2 con cero, uno y dos campos, bucles,
  switch y static anidados, y recuentos de `defer`, `finally` y destrucción:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-lngbuw5p/results.json`.
  Etiquetas explícitas hacia un ámbito exterior siguen rechazadas; no se probó
  la colisión de etiquetas generadas por clones del mismo origen simultáneamente
  anidados. El unitario de certificados y flujo pasó 82 checks sin SKIP:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-z402vjw8/results.json`.
  Las etiquetas externas existentes con limpieza, inexistentes y con selección
  vacía se rechazan en la misma guarda O0/O2:
  `A:/nsc-limitaciones-campos-evidencia/outer-label-controls-20261112T201957551497Z/results.json`.
  Los clones hermanos del mismo origen pasaron; no se probó la colisión de
  clones simultáneamente anidados:
  `A:/nsc-limitaciones-campos-evidencia/generic-label-clones-20261112T201835334389Z/results.json`.
- Etiquetas exteriores: antes del cambio, etiquetas válidas e inexistentes se
  rechazaban, incluso con selección vacía. Tras propagar el ámbito léxico, el
  runner fuente pasó O0/O2 para break/continue a etiqueta exterior, limpieza,
  anidamiento y expansión diferida; etiquetas inexistentes y saltos desde lambda
  siguen rechazándose. `break` exterior con valor y selección vacía también se
  rechaza porque ese valor no llega a análisis semántico:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-7wjqa2vv/results.json`.
  El unitario con certificados pasó 85 checks sin SKIP:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-w547xewm/results.json`.
  La pila conserva identidades y límites de función/lambda; la cobertura no
  acredita toda interacción de etiquetas con cuerpos no instanciados.

- `nameof` del descriptor: el control ordinario devolvía el nombre léxico y
  las tres formas con descriptor se rechazaban inicialmente:
  `A:/nsc-limitaciones-campos-evidencia/nameof-fields-20261112T203657581708Z/results.json`.
  Contrato adoptado: `nameof(field)` produce `field`, `nameof(field.Name)`
  produce `Name` y `nameof(field.Get(...))` produce `Get`; no es el nombre del
  campo seleccionado, que sigue siendo `field.Name`. Igual que en el control
  ordinario de llamadas, los argumentos no se evalúan ni se comprueban por el
  hecho de aparecer en `nameof`.
  El runner fuente pasó O0/O2 verificando valores exactos, X/Y heterogéneos,
  sombra, efecto lateral no ejecutado, método sin llamada y rechazos por
  miembro inexistente y fuera de ámbito:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-0_68q3wy/results.json`.
  Unitario y certificados: 85 checks sin SKIP,
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-dgnvk1k3/results.json`.
  No se acredita toda expresión anidada ni `Get` genérico en `nameof`.

- Tupla fuente de builtin: el control ordinario asignó y desestructuró
  `(int, long)` O0/O2, mientras Name/Get/Type rechazaban antes:
  `A:/nsc-limitaciones-campos-evidencia/tuple-source-fields-20261112T204900845856Z/results.json`.
  El runner posterior pasó tuplas de builtin, elementos nombrados y receptor
  incorrecto O0/O2: `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-ztzb_2jb/results.json`.
  Tuplas con elementos nominales públicos o privados todavía se rechazaban
  antes de comprobar visibilidad; sus controles ordinarios pasan:
  `A:/nsc-limitaciones-campos-evidencia/source-tuple-fields-20261112T205300431586Z/results.json`.
- La extensión posterior proyecta hojas nominales por identidad y visibilidad
  en el contexto del consumidor. El runner fuente pasó O0/O2 con Alpha.P y
  Beta.P homónimos y sobrecargas, y con tipo privado accesible dentro de su
  dueño: `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-m4rf02z2/results.json`.
  El control ordinario fuera del dueño rechaza privacidad; Fields rechaza antes
  por declaración incompleta, por lo que no demuestra allí la guarda de tipo.
  Los casos no acreditan todas las formas estructurales o binarias. Unitario y
  certificados pasaron 85 checks sin SKIP:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-binary-fields-8cym7fw0/results.json`.

- Arreglos fijos `int[N]` fuente: indexación, copia y declaración ordinarias
  ejecutaban O0/O2 y Fields rechazaba Name/Get/Type antes del cambio:
  `A:/nsc-limitaciones-campos-evidencia/fixed-array-fields-20261112T212024471955Z/results.json`.
  El runner posterior pasó O0/O2 para N=4/5, sobrecargas que discriminan la
  longitud, privacidad, receptor incompatible y N=`2+2`; `4/0` se rechazó
  tanto en el control ordinario como en Fields:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-n06yh44b/results.json`.
  Las longitudes usan el evaluador canónico, tras limitar el recorrido y excluir
  nombres y expresiones no admitidas. No se acreditan otros elementos, arreglos
  binarios ni constantes con nombre; el intento de constante nombrada también
  falló en el control ordinario.
- Un arreglo enorme `int[2147483647+1]` agotó el timeout de compilación de 30 s
  al copiarlo, tanto en Fields como en el control ordinario equivalente; Fields
  con solo Name compiló. No se ejecutaron esos binarios en la comparación final
  ni se demuestra que los dos timeouts tengan la misma causa interna:
  `A:/nsc-limitaciones-campos-evidencia/fixed-array-fields-20261112T213656230241Z/results.json`.

- Arreglos `long[N]` fuente: el control ordinario y sobrecargas `int[4]`,
  `long[4]`, `long[5]` ejecutaban O0/O2, pero Fields rechazaba por
  representación: `A:/nsc-limitaciones-campos-evidencia/fixed-array-fields-20261112T214544352677Z/results.json`.
  El runner dirigido posterior terminó 0 O0/O2 con Name/Get/Type,
  sobrecargas, visibilidad y receptor incompatible:
  `A:/nsc-limitaciones-campos-evidencia/temp/nsc-source-fields-puluq2ip/results.json`.
  El compilador SHA-256 `c12bbf5b632d1bf81e5e346ac9981a315f577560a2f23803546cc10fb2ef751e`
  permaneció estable. La repetición independiente de `int[N]` también pasó:
  `A:/nsc-limitaciones-campos-evidencia/fixed-array-fields-20261112T214803725203Z/results.json`.
  No acredita otros tipos de elemento ni arreglos binarios.

Los scripts aíslan temporales, cachés y artefactos en A:, comprueban imports
antes de ejecutar y conservan comandos, diagnósticos y hashes en los JSON.
La implementación conserva rechazos ante información insuficiente; los límites
funcionales y de evidencia se detallan en `static-field-query-phase-design.md`.
