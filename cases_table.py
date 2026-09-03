"""LA UNICA TABLA DE CASOS del corpus de fixtures y ejemplos.

POR QUE ESTE FICHERO EXISTE, y es la correccion del 2026-09-03. Esta tabla estaba escrita DOS
VECES -- en `run_cases.py` (la pasada de lld-link) y en `nslink_pipeline.py` (la de NSLink) -- y la
segunda llevaba, al final del informe, este aviso:

    "La tabla CASES de este script esta duplicada de run_cases.py a proposito (aislamiento).
     Si alguien anadio una fixture alla, hay que anadirla aqui."

Es EXACTAMENTE la misma forma de defecto que las cinco copias de la tabla de imports (docs 102 y
103), las dos tablas de enteros (doc 112), las tres cadenas del HIR (doc 106) y las dos copias de
la regla de tipado de literales (doc 115): una regla escrita dos veces con una NOTA pidiendo que
alguien se acuerde. Un aviso impreso al final de un informe no es un guardian; es una nota
adhesiva.

Y AQUI YA HABIA MORDIDO UNA VEZ. El doc 109 lo cuenta: un commit mio sobre una copia rancia
revirtio el registro de c26 en `nslink_pipeline.py`, y las dos tablas quedaron en 48 y 44. Se
detecto contando, no por un fallo -- la corrida de NSLink midio cuatro casos menos y salio verde.

EL MOTIVO REAL DE LA DUPLICACION ERA EL AISLAMIENTO: que la pasada de NSLink no dependiera del
script de la otra. Un modulo de DATOS lo respeta entero -- no importa codigo, no ejecuta nada, no
tiene dependencias -- y ademas elimina la copia. Los dos scripts siguen siendo independientes
entre si; los dos dependen de la misma tabla, que es lo que se queria.

El AVISO de conteos NO se quita: sigue comparando contra los resultados de la OTRA pasada, y eso
detecta algo distinto -- una pasada que no ejecuto todo lo que debia.

  (proyecto relativo a la raiz, etiqueta, salida esperada)

  `expected = None` significa BUILD-ONLY: compila y enlaza, no se ejecuta.
"""

IC = "tests/interop_contract"
LC = "tests/language"
CC = "tests/cc1"

# (proyecto relativo a la raiz, etiqueta, salida esperada)
#
# `expected = None` significa BUILD-ONLY: compila y enlaza, no se ejecuta. Es lo que hacia
# :build_only, y existe para el caso de las dos bibliotecas con el mismo simbolo, que se certifica
# enlazando.
CASES: list[tuple[str, str, int | None]] = [
    # --- interop contract ---
    (f"{IC}/c1_03_function_pointer_parameter/c1_03_function_pointer_parameter.nsproj", "c1_03_function_pointer_parameter", 42),
    (f"{IC}/c1_04_function_pointer_return/c1_04_function_pointer_return.nsproj", "c1_04_function_pointer_return", 42),
    (f"{IC}/c1_05_raw_pointer_cast/c1_05_raw_pointer_cast.nsproj", "c1_05_raw_pointer_cast", 42),
    (f"{IC}/c2_11_fixed_field_array/c2_11_fixed_field_array.nsproj", "c2_11_fixed_field_array", 42),
    (f"{IC}/c2_12_union/c2_12_union.nsproj", "c2_12_union", 42),
    (f"{IC}/c4_ref_out_in/c4_ref_out_in.nsproj", "c4_ref_out_in", 42),
    (f"{IC}/c6_struct_math/c6_struct_math.nsproj", "c6_struct_math", 42),
    (f"{IC}/c7_definite_assignment/c7_definite_assignment.nsproj", "c7_definite_assignment", 42),
    (f"{IC}/c8_type_root/c8_type_root.nsproj", "c8_type_root", 42),
    # La frontera de excepciones a traves de un callback de C: sale con 141 A PROPOSITO.
    (f"{IC}/c1_14_callback_exception_boundary/c1_14_callback_exception_boundary.nsproj", "c1_14_callback_exception_boundary", 141),
    # Dos bibliotecas con el mismo simbolo: se certifica ENLAZANDO, no ejecutando.
    (f"{IC}/c1_13_two_libraries_same_symbol/c1_13_two_libraries_same_symbol.nsproj", "c1_13_two_libraries_same_symbol", None),

    # --- language ---
    (f"{LC}/l1_conditional/l1_conditional.nsproj", "l1_conditional", 42),
    (f"{LC}/l2_dowhile/l2_dowhile.nsproj", "l2_dowhile", 42),
    (f"{LC}/l3_switch/l3_switch.nsproj", "l3_switch", 42),

    # --- milestone CC1 ---
    (f"{CC}/c1_constants/c1_constants.nsproj", "cc1_c1_constants", 42),
    (f"{CC}/c2_ternary_ownership/c2_ternary_ownership.nsproj", "cc1_c2_ternary_ownership", 42),
    (f"{CC}/c3_dowhile_flow/c3_dowhile_flow.nsproj", "cc1_c3_dowhile_flow", 42),
    (f"{CC}/c4_catch_continue/c4_catch_continue.nsproj", "cc1_c4_catch_continue", 42),
    (f"{CC}/c5_catch_break/c5_catch_break.nsproj", "cc1_c5_catch_break", 42),
    (f"{CC}/c6_catch_return/c6_catch_return.nsproj", "cc1_c6_catch_return", 42),
    (f"{CC}/c7_finally_switch/c7_finally_switch.nsproj", "cc1_c7_finally_switch", 42),
    (f"{CC}/c8_loop_catch_switch/c8_loop_catch_switch.nsproj", "cc1_c8_loop_catch_switch", 42),
    (f"{CC}/c9_sizeof/c9_sizeof.nsproj", "cc1_c9_sizeof", 42),
    (f"{CC}/c10_stackalloc/c10_stackalloc.nsproj", "cc1_c10_stackalloc", 42),
    (f"{CC}/c11_raw_new/c11_raw_new.nsproj", "cc1_c11_raw_new", 42),
    (f"{CC}/c12_pointer_arith/c12_pointer_arith.nsproj", "cc1_c12_pointer_arith", 42),
    (f"{CC}/c13_increment/c13_increment.nsproj", "cc1_c13_increment", 42),
    (f"{CC}/c14_compound_assign/c14_compound_assign.nsproj", "cc1_c14_compound_assign", 42),
    (f"{CC}/c15_arrow_member/c15_arrow_member.nsproj", "cc1_c15_arrow_member", 42),
    (f"{CC}/c16_enum_layout/c16_enum_layout.nsproj", "cc1_c16_enum_layout", 42),
    (f"{CC}/c17_constant_promotion/c17_constant_promotion.nsproj", "cc1_c17_constant_promotion", 42),
    (f"{CC}/c18_stackalloc_hardening/c18_stackalloc_hardening.nsproj", "cc1_c18_stackalloc_hardening", 42),
    (f"{CC}/c19_sbyte/c19_sbyte.nsproj", "cc1_c19_sbyte", 42),
    (f"{CC}/c20_property_compound/c20_property_compound.nsproj", "cc1_c20_property_compound", 42),
    (f"{CC}/c21_number_parsing/c21_number_parsing.nsproj", "cc1_c21_number_parsing", 42),
    (f"{CC}/c22_console_and_formatting/c22_console_and_formatting.nsproj", "cc1_c22_console_and_formatting", 42),
    (f"{CC}/c23_views_and_lifetimes/c23_views_and_lifetimes.nsproj", "cc1_c23_views_and_lifetimes", 42),
    (f"{CC}/c24_binary_file_io/c24_binary_file_io.nsproj", "cc1_c24_binary_file_io", 42),
    (f"{CC}/c25_binary_roundtrip/c25_binary_roundtrip.nsproj", "cc1_c25_binary_roundtrip", 42),
    (f"{CC}/c26_driver_minimo/c26_driver_minimo.nsproj", "cc1_c26_driver_minimo", 42),
    (f"{CC}/c27_algoritmos/c27_algoritmos.nsproj", "cc1_c27_algoritmos", 42),
    (f"{CC}/c28_texto/c28_texto.nsproj", "cc1_c28_texto", 42),
    (f"{CC}/c29_arena/c29_arena.nsproj", "cc1_c29_arena", 42),
    (f"{CC}/c30_contratos/c30_contratos.nsproj", "cc1_c30_contratos", 42),
    # La conversion de `string` a cadena C con NUL. La comprobacion que vale es la que le pasa la
    # cadena a `lstrlenA` de kernel32: quien decide si el NUL esta bien puesto es el SISTEMA, no
    # este programa.
    (f"{CC}/c31_cadena_c/c31_cadena_c.nsproj", "cc1_c31_cadena_c", 42),
    # LA DIVISION POR CERO ES UNA TRAMPA, y el exito de este caso es SALIR CON 129 --
    # ExitProcess(128 + 1), la razon DivisionByZero. Un 42 aqui seria el fallo.
    (f"{CC}/c32_division_por_cero/c32_division_por_cero.nsproj", "cc1_c32_division_por_cero", 129),
    # Thread.Join como punto acquire (doc 111 §2.3). Lo unico del contrato de memoria que podia
    # ser falso, y lo unico que el proyecto usa sin pensarlo.
    (f"{CC}/c33_hilos_join/c33_hilos_join.nsproj", "cc1_c33_hilos_join", 42),
    # ID 129 -- ToString() de un enum devuelve el NOMBRE. Es del compilador porque los nombres no
    # sobreviven a la compilacion.
    (f"{CC}/c34_enum_nombre/c34_enum_nombre.nsproj", "cc1_c34_enum_nombre", 42),
    # ID 132 -- sobrecargas genericas por lista de parametros. El consumidor de verdad es
    # Algorithm.Sort con y sin comparador, que hasta hoy eran Sort y SortWith.
    (f"{CC}/c35_sobrecarga_generica/c35_sobrecarga_generica.nsproj", "cc1_c35_sobrecarga_generica", 42),

    # --- ejemplos empaquetados ---
    ("examples/project_stage56_c1/Stage56C1InteropCompletion.nsproj", "project_stage56_c1", 0),
    ("examples/project_stage56_c2c4/Stage56C2C4LanguageCompletion.nsproj", "project_stage56_c2c4", 0),
    ("examples/project_stage56_c6/Stage56C6StructMath.nsproj", "project_stage56_c6", 0),
    ("examples/project_stage56_c7/project_stage56_c7.nsproj", "project_stage56_c7", 0),
    ("examples/project_stage56_c8/project_stage56_c8.nsproj", "project_stage56_c8", 0),
    # EL PROGRAMA REAL (fase C de la entrega del 2026-09-03): abre una ventana Win32, lee un
    # asset, pinta en un bucle y termina. Entra como BUILD-ONLY --expected None-- a proposito:
    #
    #   * Lo que hay que medir en cada corrida es que ENLAZA contra user32.dll y gdi32.dll, y eso
    #     lo prueba el enlace. Es la primera vez que el proyecto importa de esas dos DLLs, y hasta
    #     hoy no habia forma de saber si las bibliotecas de importacion se generaban bien para
    #     imports declarados por el USUARIO con `extern C from`.
    #   * Ejecutarlo abriria dos ventanas (una por nivel de optimizacion) en medio de una corrida
    #     de 98 trabajos en paralelo. Se ejecuta a mano, que para eso es un demo.
    ("examples/project_demo_ventana/DemoVentana.nsproj", "project_demo_ventana", None),
]
