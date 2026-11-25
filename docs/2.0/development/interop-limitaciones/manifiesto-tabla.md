### 73 casillas cotejadas (73)

| Casilla | Texto en TRACKING.md (abreviado) | Tareas | Relación |
|---|---|---|---|
| T09-003 | Conservar el nombre de símbolo y la convención de llamada elegidos por Clang. | X2-11, R2-02 | tarea asignada |
| T10-002 | Importar namespaces y sus miembros visibles. | X1-01, X4-06, R2-02 | tarea asignada |
| T10-004 | Importar variables globales C++ accesibles. | R3-03 | tarea asignada |
| T10-007 | Conservar el acceso público, protegido o privado de cada declaración. | X1-03, X3-01 | tarea asignada |
| T11-004 | Importar los campos de instancia accesibles de una clase. | X2-10 | tarea asignada |
| T11-015 | Definir la copia de un objeto C++ con constructor de copia disponible. | X2-04, X2-05 | tarea asignada |
| T11-016 | Definir el movimiento de un objeto C++ con constructor de movimiento disponible. | X2-04, X2-05 | tarea asignada |
| T11-018 | Definir la reserva y liberación de objetos creados dinámicamente. | X2-01, X2-03 | tarea asignada |
| T12-001 | Importar las clases base y sus niveles de acceso. | X3-01, X3-03 | tarea asignada |
| T12-003 | Permitir la conversión válida de derivado a base. | X3-01, X3-03 | tarea asignada |
| T12-004 | Emitir llamadas a métodos heredados con el ajuste de instancia requerido. | X2-07, X2-08 | tarea asignada |
| T12-010 | Definir la interoperación con RTTI cuando la biblioteca lo utilice. | X3-05 | tarea asignada |
| T13-001 | Auditar cómo NSC representa genéricos, argumentos de tipo e instanciaciones. | X2-09, X4-06, R2-02 | tarea asignada |
| T13-004 | Importar una plantilla de clase y sus parámetros. | X4-01, X4-02 | tarea asignada |
| T13-005 | Importar una plantilla de función y sus parámetros. | X4-03, X4-05 | tarea asignada |
| T13-021 | Definir cómo se exponen resultados `constexpr` evaluados por Clang. | X4-04 | tarea asignada |
| T13-022 | Definir cómo se consumen constantes y expresiones sujetas a evaluación en compilación. | X4-04 | tarea asignada |
| T14-002 | Definir cómo se importan valores de macros de constante verificables. | C2-07, X4-04 | tarea asignada |
| T14-003 | Definir el tratamiento de macros de función que necesitan expansión C/C++. | C2-01, C2-02, C2-06 | tarea asignada |
| T14-006 | Investigar cómo importar bitfields sin alterar su disposición ni sus reglas de acceso. | C3-05 | tarea asignada |
| T14-007 | Definir la representación de punteros a miembros de C++. | X3-06 | tarea asignada |
| T14-009 | Investigar el consumo de headers precompilados sin mezclar configuraciones. | R2-09 | tarea asignada |
| T15-002 | Identificar el modelo de excepciones empleado por Clang en el objetivo Windows seleccionado. | R3-01 | tarea asignada |
| T15-003 | Diseñar la propagación o adaptación de excepciones C++ al entrar en NativeSharp. | R3-04, R3-05 | tarea asignada |
| T15-008 | Diseñar cómo pasar una función NativeSharp como callback esperado por una biblioteca. | C4-01, R1-03 | tarea asignada |
| T15-010 | Definir la representación y vida de capturas para callbacks con estado. | C4-02, R1-03 | tarea asignada |
| T15-013 | Definir el tratamiento de excepciones que crucen el límite de un callback. | R3-06 | tarea asignada |
| T15-014 | Importar y utilizar smart pointers sin perder sus reglas de propiedad. | X4-02, X4-07 | tarea asignada |
| T15-015 | Definir la interoperación con allocators y sus instancias de estado. | X4-08, R1-04 | tarea asignada |
| T15-016 | Comprobar la transferencia de propiedad con tipos movibles y no copiables. | X2-04, X2-05 | tarea asignada |
| T15-017 | Diseñar la proyección de corutinas y de la vida útil de sus frames. | X4-08, R1-04 | tarea asignada |
| T16-002 | Identificar qué objetos de arranque y bibliotecas participan en el enlace. | R3-01 | tarea asignada |
| T16-003 | Diseñar la inicialización de constructores globales C++ antes de su uso desde NativeSharp. | R3-02 | tarea asignada |
| T16-006 | Identificar la inicialización de TLS requerida por las bibliotecas importadas. | R3-03 | tarea asignada |
| T16-007 | Comprobar datos TLS desde más de un hilo. | R3-03 | tarea asignada |
| T16-008 | Definir el orden de inicialización del runtime NativeSharp y el runtime C++. | R3-12 | tarea asignada |
| T16-012 | Comprobar carga y descarga de una DLL con objetos globales C++. | R3-10 | tarea asignada |
| T16-016 | Investigar la interoperación con LTO sin imponerla como requisito de la primera tanda. | R3-11 | si exige cambiar NSLink queda bloqueado solo ese subcaso (R3-11) |
| T17-001 | Señalar la declaración C/C++ original cuando una importación falle. | R2-04, V-07 | tarea asignada |
| T17-005 | Distinguir un header ausente de una biblioteca binaria ausente. | R2-08 | tarea asignada |
| T17-007 | Investigar cómo relacionar ubicaciones NativeSharp y C++ durante la depuración. | R2-05, V-07 | tarea asignada |
| T17-010 | Medir el costo de análisis repetido de headers con una biblioteca representativa. | R2-07 | tarea asignada |
| T17-011 | Diseñar la reutilización segura de información solo cuando la medición y la corrección lo permitan. | R2-03 | tarea asignada |
| T18-006 | Revisar las combinaciones entre características; no contar solo tipos de nodos del AST. | V-03, V-04 | tarea asignada |
| T18-011 | Compilar esa biblioteca con Clang oficial y consumirla desde NativeSharp. | V-03, V-04 | tarea asignada |
| T18-014 | Ejecutar las pruebas de regresión de NSC ajenas a interoperabilidad. | V-05 | regresión ajena a interop; V-05 |
| T19-009 | Destructores que pueden lanzar, copia y movimiento implícitos no triviales, movibles no copiables y locales que Clang no construye directamente. | X2-04, X2-05 | tarea asignada |
| T19-010 | Importar constructores plantilla (`std::unique_ptr`, `std::pair`) y los de `icu::UnicodeString`. | X4-02, X4-07 | tarea asignada |
| T19-011 | `new T` de un tipo C++ en el heap con propiedad definida. | X2-01, X2-03 | tarea asignada |
| T19-012 | Diagnóstico de acceso para miembros privados y protegidos. | X1-03, X3-01 | tarea asignada |
| T19-013 | Operadores miembro no const, heredados, de asignación, llamada, subíndice y flecha. | X2-07, X2-08 | tarea asignada |
| T19-014 | Bases virtuales (campos, conversión y construcción), referencia a la base, `typeid`/`dynamic_cast` y punteros a miembro. | X3-03, X3-05, X3-06 | tarea asignada |
| T19-015 | Plantillas miembro, plantillas de variable, constantes record/arreglo/puntero, ordenación parcial, deducción con structs NativeSharp y conjuntos… | X4-02, X4-03, X4-04, X4-05 | tarea asignada |
| T19-016 | Especializaciones C++ en la API pública de un módulo precompilado. | X2-09, X4-06, R2-02 | tarea asignada |
| T19-017 | Atrapar o convertir excepciones C++ en NativeSharp y excepciones NativeSharp dentro de callbacks. | R3-05, R3-06 | tarea asignada |
| T19-018 | `std::function`, corutinas devueltas por valor y `std::allocator::deallocate`. | X4-07, X4-08, R1-03 | tarea asignada |
| T19-019 | Runtime dinámico `/MD`, inicializadores sin CRT, `.CRT$XDU`, débiles y `/include`, y constructores globales que llaman a NativeSharp. | R3-01, R3-02, R3-12 | tarea asignada |
| T19-020 | `LoadLibrary`/`FreeLibrary`, hilos NativeSharp con TLS C++ y LTO con la biblioteca C++. | R3-03, R3-10, R3-11 | tarea asignada |
| T19-021 | Bibliotecas solo con `.pch` e interfaces de módulo `.pcm`. | R2-09 | tarea asignada |
| T19-022 | Incluir en la distribución NSLink, `lld-link`, `llvm-lib` y los encabezados de recursos de Clang, o documentar su instalación separada. | R3-08 | no se empaquetan herramientas ajenas; solo documentación (R3-08) |
| T19-023 | Ubicación C++ con línea y subrayado, miembros que faltan al enlazar, `-fdebug-prefix-map` y depuración interactiva. | R2-04, R2-05, R2-08, V-07 | tarea asignada |
| T19-024 | Reutilizar el inventario de `names` y `members` y la caché en `nsc build` para C/C++. | R2-03, R2-07 | tarea asignada |
| T19-025 | Fixtures de las combinaciones sin probar y del resto de zlib (`gz*`) e ICU. | V-02, V-03 | tarea asignada |
| T19-026 | Corregir la falla de `cpp-tamano-objeto/validar-indice-expresion` con cinco procesos; los dos validadores `cpp-variadica-preserve-none` reproducen… | V-01 | tarea asignada |
| T19-027 | Construir NSC en el commit anterior a la interoperabilidad y comparar las 36 pruebas unitarias que fallan en el CTest completo… | V-02, V-03 | línea base histórica; se reconcilia en V-02/V-03 |
| T19-031 | Tamaños de objeto ocultos (`pass_object_size`) en todas las formas y flujos de receptor C++ en plantillas precompiladas fuera de los registrados. | X2-09, X2-11, R2-02 | tarea asignada |
| T19-032 | Bloqueado por Clang 23.1.0: `preserve_all` e `intel_ocl_bicc` en C++ (`cannot mangle this calling convention yet`), variádicas C++ `preserve_none`… | R3-09 | fuera de esta autorización en lo que exige Clang 23.1 (R3-09 registra dependencia) |
| T19-033 | Lambda escrita directamente como argumento de tipo puntero a función. | C4-01, R1-03 | tarea asignada |
| T19-034 | Diagnóstico propio de TLS sin las bibliotecas del CRT y directorio TLS con NSLink (requiere autorización para modificar NSLink). | R3-03 | directorio TLS de NSLink fuera de autorización; el diagnóstico propio sí entra (R3-03) |
| T19-035 | Clases NativeSharp derivadas de clases C++: overrides que llama la biblioteca y acceso protegido desde la clase derivada. | X3-01, X3-04 | tarea asignada |
| T19-036 | Fixtures de los traslados sin evidencia: `class` en el catálogo precompilado, globales y campos estáticos C++ en plantilla precompilada, campos… | C3-10, X4-06, R2-02, R3-03, R3-04 | tarea asignada |
| T19-037 | Línea base de las 189 pruebas de NSC que fallan o no se ejecutan: tres símbolos sin definir de `ninja -k 0` (83 sin ejecutable),… | V-02, V-03 | línea base histórica; se reconcilia en V-02/V-03 |
| T19-043 | Unions C++ con miembros de vida no trivial (constructor o destructor no trivial), también anónimas: hoy no se importan sus miembros, porque hay… | X2-10 | tarea asignada |

### 16 casillas abiertas adicionales (16)

| Casilla | Texto en TRACKING.md (abreviado) | Tareas | Relación |
|---|---|---|---|
| T10-006 | Importar campos estáticos de una clase. | R3-03, X4-06 | tarea asignada |
| T11-002 | Definir la representación NativeSharp de una instancia de clase C++. | X1-01, X4-06, R2-02 | tarea asignada |
| T11-005 | Importar los métodos de instancia accesibles de una clase. | X1-03, X1-04, X3-04 | tarea asignada |
| T11-007 | Importar el destructor y sus restricciones de acceso. | X2-02, R3-04 | tarea asignada |
| T12-006 | Emitir despacho virtual respetando el ABI de Clang para el objetivo elegido. | X3-04, X4-06 | tarea asignada |
| T12-009 | Representar las reglas y ajustes de herencia virtual. | X3-01, X3-03 | tarea asignada |
| T12-011 | Definir las conversiones dinámicas compatibles con el modelo importado. | X3-05 | tarea asignada |
| T13-007 | Diseñar la representación de un tipo NativeSharp como argumento de template C++. | X4-05 | tarea asignada |
| T13-011 | Importar los miembros de la especialización resultante. | X4-06, R3-04 | tarea asignada |
| T13-020 | Comprobar la misma especialización solicitada desde dos módulos NativeSharp. | X4-06, R2-02 | tarea asignada |
| T14-010 | Diseñar el consumo de módulos C++ compatibles con la versión fijada de Clang. | R2-09 | tarea asignada |
| T16-009 | Auditar el estado del runtime NativeSharp cuando intervienen varias DLL. | R3-01 | tarea asignada |
| T17-008 | Comprobar la información de depuración en una llamada que cruza el límite de lenguaje. | R2-05, V-07 | tarea asignada |
| T18-009 | Compilar esa biblioteca con Clang oficial y consumirla desde NativeSharp. | V-04 | tarea asignada |
| T18-013 | Consumir desde NativeSharp especializaciones de esa biblioteca. | X2-04, X4-03, X4-07 | tarea asignada |
| T18-023 | Declarar terminado el objetivo solo cuando el inventario acordado y sus combinaciones estén cubiertos con evidencia. | V-03, V-05, V-06 | tarea asignada |

### Índice inverso: tarea → casillas

| Tarea | Casillas |
|---|---|
| P0-01 | — (tarea propia del plan, sin casilla) |
| P0-02 | — (tarea propia del plan, sin casilla) |
| P0-03 | — (tarea propia del plan, sin casilla) |
| P0-04 | — (tarea propia del plan, sin casilla) |
| P0-05 | — (tarea propia del plan, sin casilla) |
| C1-01 | — (tarea propia del plan, sin casilla) |
| C1-02 | — (tarea propia del plan, sin casilla) |
| C1-03 | — (tarea propia del plan, sin casilla) |
| C1-04 | — (tarea propia del plan, sin casilla) |
| C1-05 | — (tarea propia del plan, sin casilla) |
| C1-06 | — (tarea propia del plan, sin casilla) |
| C2-01 | T14-003 |
| C2-02 | T14-003 |
| C2-03 | — (tarea propia del plan, sin casilla) |
| C2-04 | — (tarea propia del plan, sin casilla) |
| C2-05 | — (tarea propia del plan, sin casilla) |
| C2-06 | T14-003 |
| C2-07 | T14-002 |
| C2-08 | — (tarea propia del plan, sin casilla) |
| C2-09 | — (tarea propia del plan, sin casilla) |
| C3-01 | — (tarea propia del plan, sin casilla) |
| C3-02 | — (tarea propia del plan, sin casilla) |
| C3-03 | — (tarea propia del plan, sin casilla) |
| C3-04 | — (tarea propia del plan, sin casilla) |
| C3-05 | T14-006 |
| C3-06 | — (tarea propia del plan, sin casilla) |
| C3-07 | — (tarea propia del plan, sin casilla) |
| C3-08 | — (tarea propia del plan, sin casilla) |
| C3-09 | — (tarea propia del plan, sin casilla) |
| C4-01 | T15-008, T19-033 |
| C4-02 | T15-010 |
| C4-03 | — (tarea propia del plan, sin casilla) |
| C4-04 | — (tarea propia del plan, sin casilla) |
| C4-05 | — (tarea propia del plan, sin casilla) |
| X1-01 | T10-002, T11-002 |
| X1-02 | — (tarea propia del plan, sin casilla) |
| X1-03 | T10-007, T19-012, T11-005 |
| X1-04 | T11-005 |
| X1-05 | — (tarea propia del plan, sin casilla) |
| X1-06 | — (tarea propia del plan, sin casilla) |
| X2-01 | T11-018, T19-011 |
| X2-02 | T11-007 |
| X2-03 | T11-018, T19-011 |
| X2-04 | T11-015, T11-016, T15-016, T19-009, T18-013 |
| X2-05 | T11-015, T11-016, T15-016, T19-009 |
| X2-06 | — (tarea propia del plan, sin casilla) |
| X2-07 | T12-004, T19-013 |
| X2-08 | T12-004, T19-013 |
| X2-09 | T13-001, T19-016, T19-031 |
| X3-01 | T10-007, T12-001, T12-003, T19-012, T19-035, T12-009 |
| X3-02 | — (tarea propia del plan, sin casilla) |
| X3-03 | T12-001, T12-003, T19-014, T12-009 |
| X3-04 | T19-035, T11-005, T12-006 |
| X3-05 | T12-010, T19-014, T12-011 |
| X3-06 | T14-007, T19-014 |
| X3-07 | — (tarea propia del plan, sin casilla) |
| X4-01 | T13-004 |
| X4-02 | T13-004, T15-014, T19-010, T19-015 |
| X4-03 | T13-005, T19-015, T18-013 |
| X4-04 | T13-021, T13-022, T14-002, T19-015 |
| X4-05 | T13-005, T19-015, T13-007 |
| X4-06 | T10-002, T13-001, T19-016, T19-036, T10-006, T11-002, T12-006, T13-011, T13-020 |
| X4-07 | T15-014, T19-010, T19-018, T18-013 |
| R1-01 | — (tarea propia del plan, sin casilla) |
| R1-02 | — (tarea propia del plan, sin casilla) |
| R1-03 | T15-008, T15-010, T19-018, T19-033 |
| R1-04 | T15-015, T15-017 |
| R1-05 | — (tarea propia del plan, sin casilla) |
| R2-01 | — (tarea propia del plan, sin casilla) |
| R2-02 | T09-003, T10-002, T13-001, T19-016, T19-031, T19-036, T11-002, T13-020 |
| R2-03 | T17-011, T19-024 |
| R2-04 | T17-001, T19-023 |
| R2-05 | T17-007, T19-023, T17-008 |
| R2-06 | — (tarea propia del plan, sin casilla) |
| R3-01 | T15-002, T16-002, T19-019, T16-009 |
| R3-02 | T16-003, T19-019 |
| R3-03 | T10-004, T16-006, T16-007, T19-020, T19-034, T19-036, T10-006 |
| R3-04 | T15-003, T19-036, T11-007, T13-011 |
| R3-05 | T15-003, T19-017 |
| R3-06 | T15-013, T19-017 |
| R3-07 | — (tarea propia del plan, sin casilla) |
| R3-08 | T19-022 |
| R3-09 | T19-032 |
| V-01 | T19-026 |
| V-02 | T19-025, T19-027, T19-037 |
| V-03 | T18-006, T18-011, T19-025, T19-027, T19-037, T18-023 |
| V-04 | T18-006, T18-011, T18-009 |
| V-05 | T18-014, T18-023 |
| V-06 | T18-023 |
| C5-01 | — (tarea propia del plan, sin casilla) |
| C5-02 | — (tarea propia del plan, sin casilla) |
| C5-03 | — (tarea propia del plan, sin casilla) |
| C5-04 | — (tarea propia del plan, sin casilla) |
| C5-05 | — (tarea propia del plan, sin casilla) |
| C5-06 | — (tarea propia del plan, sin casilla) |
| C5-07 | — (tarea propia del plan, sin casilla) |
| C5-08 | — (tarea propia del plan, sin casilla) |
| R3-10 | T16-012, T19-020 |
| R3-11 | T16-016, T19-020 |
| R3-12 | T16-008, T19-019 |
| R2-07 | T17-010, T19-024 |
| R2-08 | T17-005, T19-023 |
| R2-09 | T14-009, T19-021, T14-010 |
| V-07 | T17-001, T17-007, T19-023, T17-008 |
| X2-10 | T11-004, T19-043 |
| X4-08 | T15-015, T15-017, T19-018 |
| C3-10 | T19-036 |
| X2-11 | T09-003, T19-031 |

Tareas citadas en la matriz que no existen en el plan: ninguna.
