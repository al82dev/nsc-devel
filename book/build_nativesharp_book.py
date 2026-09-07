from __future__ import annotations

import re
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "book" / "NativeSharp_El_Lenguaje_de_Programacion_ES.base.md"
OUTPUT = ROOT / "book" / "NativeSharp_El_Lenguaje_de_Programacion_ES.md"
EXAMPLES = ROOT / "book_examples"
LIB = ROOT / "lib"
DROZDEK = ROOT / "book" / "Drozdek_en_NativeSharp_ES.md"
DEITEL = ROOT / "book" / "Deitel_en_NativeSharp_ES.md"
DROZDEK_CORE = ROOT / "book_examples" / "27_drozdek_core_structures"

FORMAT_STYLE = (
    "{BasedOnStyle: Microsoft, ColumnLimit: 120, IndentWidth: 4, UseTab: Never, "
    "SpaceBeforeParens: ControlStatements, BreakBeforeBraces: Allman, "
    "NamespaceIndentation: All, ReflowComments: true}"
)


def find_clang_format() -> str:
    discovered = shutil.which("clang-format")
    candidates = [
        discovered,
        r"C:\clang-llvm\bin\clang-format.exe",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise RuntimeError("clang-format is required to build the editorial listings")


CLANG_FORMAT = find_clang_format()


def remove_comments(source: str) -> str:
    result: list[str] = []
    index = 0
    state = "normal"
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if state == "normal":
            if char == '"':
                state = "string"
                result.append(char)
            elif char == "'":
                state = "char"
                result.append(char)
            elif char == "/" and next_char == "/":
                state = "line_comment"
                result.append(" ")
                index += 1
            elif char == "/" and next_char == "*":
                state = "block_comment"
                result.append(" ")
                index += 1
            else:
                result.append(char)
        elif state == "string":
            result.append(char)
            if char == "\\" and next_char:
                result.append(next_char)
                index += 1
            elif char == '"':
                state = "normal"
        elif state == "char":
            result.append(char)
            if char == "\\" and next_char:
                result.append(next_char)
                index += 1
            elif char == "'":
                state = "normal"
        elif state == "line_comment":
            if char == "\n":
                result.append("\n")
                state = "normal"
        elif state == "block_comment":
            if char == "*" and next_char == "/":
                result.append(" ")
                state = "normal"
                index += 1
            elif char == "\n":
                result.append("\n")
        index += 1
    return "".join(result)


def format_nativesharp(source: str, filename: str, *, without_comments: bool = False) -> str:
    if without_comments:
        source = remove_comments(source)
    process = subprocess.run(
        [CLANG_FORMAT, f"--assume-filename={Path(filename).stem}.cs", f"-style={FORMAT_STYLE}"],
        input=source,
        text=True,
        encoding="utf-8",
        capture_output=True,
        check=False,
    )
    if process.returncode != 0:
        raise RuntimeError(f"clang-format failed for {filename}: {process.stderr.strip()}")
    formatted = process.stdout.rstrip()
    formatted = re.sub(r"(?m)^(\s*)(get|set)\s*\{$", r"\1\2\n\1{", formatted)
    too_long = [(number, len(line)) for number, line in enumerate(formatted.splitlines(), start=1) if len(line) > 120]
    if too_long:
        detail = ", ".join(f"line {number}: {length}" for number, length in too_long[:5])
        raise RuntimeError(f"formatted code exceeds 120 columns in {filename}: {detail}")
    return formatted


def format_project_xml(source: str, filename: str) -> str:
    result: list[str] = []
    for raw_line in source.rstrip().splitlines():
        if len(raw_line) <= 120:
            result.append(raw_line)
            continue
        match = re.fullmatch(r'(\s*)<([A-Za-z0-9_.-]+)\s+([A-Za-z0-9_.-]+)="([^"]+)"\s*/>', raw_line)
        if not match:
            raise RuntimeError(f"project XML exceeds 120 columns in {filename}: {raw_line}")
        indent, element, attribute, value = match.groups()
        result.extend([f"{indent}<{element}", f'{indent}  {attribute}="{value}"', f"{indent}/>" ])
    too_long = [(number, len(line)) for number, line in enumerate(result, start=1) if len(line) > 120]
    if too_long:
        detail = ", ".join(f"line {number}: {length}" for number, length in too_long[:5])
        raise RuntimeError(f"formatted project XML exceeds 120 columns in {filename}: {detail}")
    return "\n".join(result)


@dataclass(frozen=True)
class CaseStudy:
    directory: str
    title: str
    problem: str
    design: str
    audit: str


CASES = [
    CaseStudy(
        "01_rational_numbers",
        "Números racionales",
        "Construir un tipo numérico que preserve la fracción exacta, normalice signos y reduzca cada resultado mediante el máximo común divisor.",
        "La clase concentra una invariante algebraica, sobrecarga aritmética y comparaciones, convierte a texto y usa una excepción específica para el denominador cero. Dos series ejercitan el tipo fuera de una demostración trivial.",
        "Compruebe que el denominador siempre queda positivo, que cero se normaliza, que cada resultado temporal tiene un dueño y que dividir por un racional cero no produce un valor silencioso.",
    ),
    CaseStudy(
        "02_payroll_oop",
        "Nómina polimórfica",
        "Calcular el pago semanal de empleados asalariados, por hora, por comisión y por comisión con salario base sin convertir el reporte en una cadena de pruebas de tipo.",
        "Una clase abstracta define el contrato y cuatro clases concretas implementan la fórmula. `Money` separa centavos de formato; el reporte consume un array heterogéneo mediante despacho virtual.",
        "Siga la destrucción de empleados, resultados `Money` y strings devueltos como `owned`. Verifique también que las reglas de horas extra y comisión permanezcan en la clase que posee la política.",
    ),
    CaseStudy(
        "03_searching_objects",
        "Búsqueda sobre objetos",
        "Comparar búsqueda lineal, binaria, binaria recursiva y exponencial sobre un conjunto de estudiantes, midiendo comparaciones y precondiciones.",
        "Los algoritmos comparten un modelo de datos y producen un reporte uniforme. La búsqueda binaria no se presenta aislada: se muestra la necesidad de ordenar y se contrasta el costo observado.",
        "Pruebe claves ausentes, primer y último elemento y un conjunto vacío. Revise qué algoritmos exigen orden y cuáles conservan el arreglo.",
    ),
    CaseStudy(
        "04_sorting_laboratory",
        "Laboratorio de ordenamiento",
        "Comparar siete algoritmos de ordenamiento sobre objetos y dos criterios sin duplicar cada algoritmo por criterio.",
        "Una jerarquía de sorters separa la estrategia; otra jerarquía separa la comparación. El reporte reconstruye el dataset, cuenta comparaciones, swaps y movimientos, y certifica que la salida está ordenada.",
        "Observe estabilidad, complejidad, memoria auxiliar y peor caso. Audite que cada corrida trabaje sobre una copia equivalente y destruya tanto dataset como estrategias.",
    ),
    CaseStudy(
        "05_singly_linked_list",
        "Lista simplemente enlazada genérica",
        "Implementar inserción, búsqueda, eliminación y recorrido sin delegar la invariante de enlaces a una colección existente.",
        "La lista genérica separa nodo y contenedor. El contenedor conserva cabeza, cola y cantidad; las operaciones actualizan los tres de manera coordinada.",
        "Dibuje los enlaces antes y después de quitar cabeza, cola y nodo medio. Compruebe que `Clear` destruye cada nodo una sola vez y deja el objeto reutilizable.",
    ),
    CaseStudy(
        "06_stack_expression_evaluator",
        "Pila y evaluador de expresiones",
        "Evaluar expresiones infijas con precedencia, paréntesis, números de varios dígitos y recuperación controlada ante entrada inválida.",
        "Una pila genérica redimensionable sostiene valores y operadores. El evaluador convierte la gramática operacional en código, reduce según precedencia y lanza una excepción de dominio con estado de error.",
        "Trace una expresión carácter por carácter. Pruebe paréntesis desbalanceados, división por cero, operador sin operando y una expresión vacía.",
    ),
    CaseStudy(
        "07_queue_service_simulation",
        "Simulación de una cola de servicio",
        "Modelar llegadas, espera, atención y utilización de un escritorio durante una secuencia discreta de ticks.",
        "Una cola circular evita desplazar elementos. La simulación separa clientes, dataset y estadísticas para que la política pueda cambiar sin alterar la estructura.",
        "Compruebe wrap-around, cola llena, tick sin cliente y cierre con trabajo pendiente. Distinga tiempo medio de espera de utilización del servidor.",
    ),
    CaseStudy(
        "08_binary_search_tree",
        "Árbol binario de búsqueda",
        "Mantener estudiantes ordenados por clave con inserción, búsqueda, recorridos y eliminación de los tres casos estructurales.",
        "El árbol encapsula la raíz y el tamaño; los nodos mantienen enlaces. La eliminación sustituye mediante sucesor cuando hay dos hijos y conserva el orden global.",
        "Valide duplicados, hoja, nodo con un hijo y raíz con dos. Después de cada operación, el recorrido inorder debe permanecer ordenado.",
    ),
    CaseStudy(
        "09_doubly_linked_list",
        "Lista doblemente enlazada",
        "Obtener recorrido en ambos sentidos e inserciones y eliminaciones locales con enlaces anterior y siguiente.",
        "La redundancia de enlaces reduce costo de navegación pero duplica la invariante. Cada mutación debe actualizar pares simétricos y extremos.",
        "Revise las cuatro fronteras: vacía, un nodo, cabeza y cola. Una mitad actualizada produce corrupción que puede no aparecer hasta recorrer en sentido contrario.",
    ),
    CaseStudy(
        "10_circular_linked_list_josephus",
        "Lista circular y problema de Josephus",
        "Resolver eliminación periódica sobre un círculo sin transformar el problema en índices de un array.",
        "La estructura hace que el sucesor de la cola sea la cabeza. El solver consume nodos siguiendo la semántica del círculo y deja visible el orden de eliminación.",
        "Pruebe paso uno, un participante y pasos mayores que el tamaño. Compruebe que el último enlace no retenga un ciclo después de limpiar.",
    ),
    CaseStudy(
        "11_deque_and_palindrome",
        "Deque y análisis de palíndromos",
        "Usar una estructura de doble extremo tanto como cola de trabajo como para comparar caracteres simétricos.",
        "El deque se implementa sobre array circular y comparte la aritmética de índices entre frente y fondo. El analizador es un consumidor que no conoce el almacenamiento.",
        "Audite wrap-around, crecimiento, longitud impar, texto vacío y normalización. Un deque correcto debe conservar orden tras combinaciones de ambos extremos.",
    ),
    CaseStudy(
        "12_avl_tree",
        "Árbol AVL",
        "Conservar búsqueda logarítmica mediante alturas y rotaciones después de insertar y eliminar.",
        "El árbol implementa rotaciones izquierda, derecha y dobles, recalcula alturas y expone una validación de balance. El programa muestra la estructura lateralmente tras operaciones reales.",
        "Fuerce los cuatro patrones de rotación. Verifique simultáneamente orden BST, alturas almacenadas y factor de balance; comprobar sólo uno puede ocultar el bug.",
    ),
    CaseStudy(
        "13_binary_heap_priority_queue",
        "Heap binario y planificador por prioridad",
        "Elegir repetidamente el trabajo más prioritario sin ordenar toda la colección después de cada inserción.",
        "La cola prioritaria usa heap en array y operaciones sift-up/sift-down. El planificador demuestra que la estructura conserva una política útil, no sólo enteros abstractos.",
        "Pruebe prioridades iguales, crecimiento y extracción hasta vacío. La invariante local padre-hijo debe mantenerse después de cada swap.",
    ),
    CaseStudy(
        "14_trie_dictionary",
        "Trie de diccionario",
        "Guardar palabras por prefijo, consultar pertenencia y enumerar coincidencias sin comparar cada palabra completa.",
        "Cada nivel representa un carácter y los caminos comparten prefijos. El dataset y la impresión permiten observar nodos internos que no son palabras completas.",
        "Pruebe una palabra que es prefijo de otra, prefijo inexistente y caracteres de borde. La limpieza debe recorrer todo el árbol de hijos.",
    ),
    CaseStudy(
        "15_hash_table",
        "Tabla hash de símbolos",
        "Resolver nombres con colisiones controladas y crecimiento sin confundir hash con identidad.",
        "La tabla distribuye símbolos en buckets enlazados. La igualdad decide identidad después del hash; inserción, lookup y eliminación conservan cadenas.",
        "Fuerce colisiones, actualizaciones y borrado en cabeza y medio de bucket. Una función hash pobre debe degradar rendimiento, no corrección.",
    ),
    CaseStudy(
        "16_graph_bfs_dfs",
        "Grafo, BFS y DFS",
        "Representar ciudades y conexiones, recorrer por amplitud y profundidad y reconstruir un camino no ponderado.",
        "Una matriz de adyacencia hace explícito el costo espacial. BFS usa cola y predecesores; DFS se muestra iterativo con pila y recursivo.",
        "Compare órdenes, pruebe vértices desconectados y reconstruya el camino desde predecesores. Distinga visitar de descubrir para no insertar duplicados sin límite.",
    ),
    CaseStudy(
        "17_dijkstra_shortest_paths",
        "Caminos mínimos con Dijkstra",
        "Encontrar el costo mínimo entre ciudades con pesos no negativos y reconstruir la ruta.",
        "El solver mantiene distancia tentativa, conjunto visitado y predecesor. La selección lineal del mínimo prioriza claridad; después puede sustituirse por heap.",
        "Rechace conceptualmente pesos negativos, proteja el infinito contra suma y pruebe destino inalcanzable. La ruta se reconstruye sólo si existe distancia finita.",
    ),
    CaseStudy(
        "18_topological_sort",
        "Orden topológico",
        "Programar tareas con dependencias dirigidas y detectar cuándo un ciclo impide un orden válido.",
        "El algoritmo de Kahn calcula grados de entrada, encola tareas disponibles y reduce dependencias. El resultado no es necesariamente único.",
        "Pruebe dos órdenes válidos, grafo vacío y ciclo. Producir menos tareas que el total es evidencia de ciclo, no un resultado parcial exitoso.",
    ),
    CaseStudy(
        "19_kruskal_mst_union_find",
        "Kruskal y union-find",
        "Construir un árbol de expansión mínimo eligiendo aristas por peso sin crear ciclos.",
        "Kruskal ordena aristas y union-find mantiene componentes con representantes. El ejemplo conecta estructura auxiliar, ordenamiento y propiedad global.",
        "Compruebe conectividad, pesos repetidos y grafo desconectado. El resultado de un grafo desconectado es bosque, salvo que el contrato exija error.",
    ),
    CaseStudy(
        "20_dynamic_programming_knapsack",
        "Mochila 0/1 con programación dinámica",
        "Maximizar valor bajo una capacidad cuando cada artículo puede elegirse como máximo una vez.",
        "La tabla dinámica registra subproblemas por prefijo de artículos y capacidad. La reconstrucción distingue el valor óptimo de la selección que lo alcanza.",
        "Pruebe capacidad cero, artículo demasiado pesado y empates. Explique por qué recorrer capacidades en el orden equivocado cambia 0/1 por mochila ilimitada.",
    ),
    CaseStudy(
        "21_dynamic_programming_lcs",
        "Subsecuencia común más larga",
        "Encontrar una secuencia ordenada común a dos textos sin exigir contigüidad.",
        "La matriz dinámica compara prefijos y la reconstrucción camina decisiones. El caso muestra que obtener sólo la longitud es una especificación incompleta para muchas aplicaciones.",
        "Pruebe texto vacío, caracteres repetidos y soluciones múltiples. No confunda subsecuencia con substring.",
    ),
    CaseStudy(
        "22_dynamic_programming_edit_distance",
        "Distancia de edición de Levenshtein",
        "Medir el mínimo de inserciones, eliminaciones y sustituciones para transformar un texto en otro.",
        "La recurrencia llena una tabla por prefijos y usa bordes como costo de convertir desde o hacia vacío. El programa contrasta pares con resultados conocidos.",
        "Pruebe simetría, igualdad, vacío y caracteres repetidos. Si se agregan costos distintos, documente que la métrica cambia.",
    ),
    CaseStudy(
        "23_polynomial_algebra",
        "Álgebra de polinomios",
        "Representar coeficientes, sumar, restar, multiplicar, derivar y evaluar sin esconder la complejidad de cada operación.",
        "El indexador abstrae potencias, los operadores producen polinomios nuevos y Horner evalúa en tiempo lineal. La impresión materializa strings temporales bajo RAII.",
        "Revise coeficientes cero, polinomio constante, grado resultante y ownership de cada resultado algebraico. Compare Horner con potencias repetidas.",
    ),
    CaseStudy(
        "24_vector_matrix_algebra",
        "Vectores y matrices 3x3",
        "Componer transformaciones lineales y operar vectores con una interfaz algebraica legible.",
        "`Vector3` implementa suma, resta, producto punto y cruz. `Matrix3` almacena nueve valores, compone matrices y transforma vectores. El programa conserva todos los resultados para imprimirlos y destruirlos.",
        "Verifique orden de multiplicación, matriz identidad, signos del producto cruz y layout de índices. Audite que cada objeto creado se destruya una sola vez.",
    ),
    CaseStudy(
        "25_drozdek_advanced_suite",
        "Laboratorio avanzado de Drozdek",
        "Cerrar los huecos algorítmicos que quedan al comparar el temario completo de Drozdek con los primeros veinticuatro estudios de caso.",
        "Nueve módulos ejecutables reúnen ordenamientos no comparativos, skip lists, árboles autoajustables y multiway, grafos avanzados, hashing, compresión, administración de memoria, búsqueda de texto y los casos clásicos faltantes.",
        "Cada módulo calcula resultados conocidos y contribuye a un checksum global. Audite precondiciones, invariantes y liberación de arrays y nodos antes de sustituir los datasets pequeños por entradas grandes.",
    ),
    CaseStudy(
        "26_deitel_live_code_native",
        "Live code al estilo Deitel, con filosofía NativeSharp",
        "Enseñar mediante aplicaciones completas que evolucionan paso a paso sin importar al mundo nativo las suposiciones del CLR.",
        "Seis demostraciones integran cuenta bancaria en centavos, libro de calificaciones, cartas y craps reproducibles, tiempo validado, archivo de crédito binario y trabajo paralelo con hilos y atomics.",
        "Pruebe invariantes, casos de error, semillas deterministas, layout de archivo, orden de destrucción y join de hilos. El checksum global evita que una demostración sólo produzca salida plausible.",
    ),
    CaseStudy(
        "27_drozdek_core_structures",
        "Estructuras fundamentales de Drozdek, visibles y canónicas",
        "Presentar listas, pilas, colas, árboles, grafos y hashing como implementaciones completas, no como resúmenes que obligan al lector a perseguir referencias cruzadas.",
        "Ocho fuentes independientes exponen la estructura, sus invariantes, operaciones, algoritmos y destrucción determinista. Un programa conductor ejecuta todas y certifica un checksum común.",
        "Fuerce casos frontera, crecimiento, colisiones, rotaciones y eliminaciones. Compare el checksum y la salida entre `-O0` y `-O2`, y valide también orden, balance y propiedad de memoria.",
    ),
]


@dataclass(frozen=True)
class Tutorial:
    directory: str
    title: str
    focus: str
    csharp_bridge: str
    exit_code: int


TUTORIALS = [
    Tutorial("tutorial_01_hello_mundo", "Programa, `Main` y consola", "La unidad mínima de programa, la referencia a Core, strings nativos y salida.", "La forma resulta familiar para C#, pero el string temporal ya introduce vida nativa y destrucción.", 29),
    Tutorial("tutorial_02_variables_tipos_operadores", "Variables, tipos y operadores", "Enteros con signo y sin signo, flotantes, booleanos, conversiones y operadores.", "Sustituye la intuición del CLR por anchos ABI concretos y promociones verificables.", 410265574),
    Tutorial("tutorial_03_control_de_flujo", "Control de flujo", "Condiciones, loops, `break`, `continue`, condicional, `do` y `switch` en la superficie actual.", "El control se parece a C#, pero su interacción con limpieza RAII y ownership forma parte de la semántica.", 348),
    Tutorial("tutorial_04_funciones_y_recursion", "Funciones, parámetros y recursión", "Métodos estáticos, stack frames, retorno, recursión y parámetros por referencia.", "NativeSharp incluye actualmente `ref`, `out` e `in`; el texto histórico que afirmaba lo contrario se excluye.", 133),
    Tutorial("tutorial_05_arrays_y_strings", "Arrays, strings y memoria contigua", "Longitud, indexación, recorrido, copia, inversión y strings con longitud explícita.", "Un array o string no queda vivo por intervención de un GC; cada dueño debe identificarse.", 212),
    Tutorial("tutorial_06_structs", "Structs y semántica de valor", "Campos inline, copia independiente, métodos y arrays de valores.", "A diferencia de una clase C#, no hay boxing implícito; el layout nativo permanece visible.", 227),
    Tutorial("tutorial_07_clases_y_objetos", "Clases, identidad y destructores", "Construcción, campos privados, métodos, alias de referencia y destrucción.", "Es el punto donde un programador de C# debe abandonar la suposición de alcance administrado.", 61),
    Tutorial("tutorial_08_herencia_y_polimorfismo", "Herencia y polimorfismo", "Bases abstractas, métodos virtuales, overrides y destrucción a través de una referencia base.", "La sintaxis es cercana a C#, pero el despacho y la destrucción son enteramente nativos.", 83),
    Tutorial("tutorial_09_interfaces", "Interfaces", "Contratos múltiples, implementación y despacho por interfaz.", "Las interfaces no son metadata CLR: el compilador genera tablas y ajustes de receptor nativos.", 61),
    Tutorial("tutorial_10_propiedades_indexadores_operadores", "Propiedades, indexadores y operadores", "Superficies expresivas para estado, colecciones y tipos algebraicos.", "Se conserva la legibilidad de C# sin esconder asignación, retorno de dueños o costo de objetos nuevos.", 98),
    Tutorial("tutorial_11_rtti_is_as", "RTTI con `is` y `as`", "Pruebas de tipo y casts comprobados sobre jerarquías nativas.", "No hay reflection ni boxing del CLR; las consultas usan descriptores de NativeSharp.", 7),
    Tutorial("tutorial_12_ownership_raii_owned_borrowed_move", "Ownership y RAII", "La diferencia central del lenguaje: dueño, préstamo, transferencia y limpieza de alcance.", "Convierte en tipos y diagnósticos las obligaciones que C# normalmente entrega al GC.", 4),
    Tutorial("tutorial_13_ownership_en_funciones", "Ownership en firmas", "Retornos owned, parámetros borrowed, consumo y `move` entre funciones.", "En una API NativeSharp, propiedad y tiempo de vida son parte de la firma pública.", 142),
    Tutorial("tutorial_14_generics_basicos", "Genéricos AOT", "Tipos y métodos parametrizados, instancias cerradas y especialización.", "La build actual también tiene constraints `where`; se elimina el comentario histórico que los negaba.", 77),
    Tutorial("tutorial_15_generic_collections_tour", "Colecciones genéricas", "List, stack, queue, dictionary y set con argumentos valor y referencia.", "Las operaciones de colección trasladan ownership de forma visible, algo que una colección administrada no exige.", 84),
    Tutorial("tutorial_16_foreach_sobre_colecciones", "`foreach` estructural", "Recorrido de arrays y enumeradores por el protocolo `GetEnumerator`/`MoveNext`/`Current`.", "No depende de `IEnumerable<T>` ni de un runtime administrado.", 108),
    Tutorial("tutorial_17_delegates_y_lambdas", "Delegados, lambdas y cierres", "Funciones como valores, métodos de instancia y captura de estado.", "La forma es conocida para C#, pero el cierre tiene representación y vida nativas.", 74),
    Tutorial("tutorial_18_eventos", "Eventos", "Publicación, suscripción, múltiples observadores y desuscripción.", "La restricción de emisión conserva el modelo de C#, implementado con delegados nativos.", 12),
    Tutorial("tutorial_19_excepciones", "Excepciones y limpieza", "Throw, catches tipados, finally y destrucción durante unwinding.", "No existe un GC que rescate recursos omitidos; RAII debe cerrar todos los caminos excepcionales.", 233),
    Tutorial("tutorial_20_recursion_clasica", "Recursión clásica", "Factorial, Fibonacci, Euclides y recorrido recursivo con análisis de costo.", "Cada llamada consume stack nativo real; profundidad y complejidad dejan de ser abstracciones académicas.", 227),
    Tutorial("tutorial_21_busqueda_y_ordenamiento", "Búsqueda y ordenamiento", "Búsqueda lineal y binaria, selección e inserción sobre arrays.", "El tutorial conecta control de flujo con precondiciones y complejidad antes del laboratorio grande.", 34),
    Tutorial("tutorial_22_linked_list_stack_queue_manual", "Lista, pila y cola manuales", "Nodos, enlaces, extremos e invariantes de estructuras lineales.", "La ausencia de GC obliga a implementar también la limpieza, no sólo las operaciones felices.", 77),
    Tutorial("tutorial_23_huffman_priority_queue", "Huffman con cola de prioridad", "Frecuencias, heap, construcción de árbol y codificación prefija.", "Integra clases, ownership, estructuras y algoritmo en un programa que ya no cabe en una diapositiva.", 42),
]


def render_tutorials() -> str:
    by_directory = {tutorial.directory: tutorial for tutorial in TUTORIALS}
    directories = sorted(path for path in (ROOT / "examples").glob("tutorial_*") if path.is_dir())
    missing = sorted(set(by_directory) - {path.name for path in directories})
    if missing:
        raise RuntimeError(f"tutorial directories missing: {missing}")
    sections: list[str] = []
    for number, directory in enumerate(directories, start=1):
        tutorial = by_directory[directory.name]
        project = next(directory.glob("*.nsproj"))
        sources = sorted(directory.glob("*.ns"), key=lambda path: (path.name == "Main.ns", path.name.lower()))
        sections.extend(
            [
                f"### 35.{number} {tutorial.title}",
                "",
                f"**Objetivo.** {tutorial.focus}",
                "",
                f"**Puente desde C#.** {tutorial.csharp_bridge}",
                "",
                "**Estado comprobado.** Compila en `-O0` y `-O2`; ambas ejecuciones producen la misma salida y "
                f"el mismo checksum de retorno (`{tutorial.exit_code}`). Un retorno distinto de cero es deliberado en esta "
                "serie: `Main` devuelve resultados calculados para que el ejecutable sea autoauditable.",
                "",
                "```powershell",
                "nsc --emit-project-oop-exe `",
                f"    examples/{directory.name}/{project.name} `",
                "    -O2 `",
                f"    -o {directory.name}.exe",
                f"./{directory.name}.exe",
                "```",
                "",
                "#### Proyecto",
                "",
                f"##### `{project.relative_to(ROOT).as_posix()}`",
                "",
                "```xml",
                format_project_xml(project.read_text(encoding="utf-8"), project.name),
                "```",
                "",
                "#### Fuente editorial",
                "",
            ]
        )
        for source in sources:
            formatted = format_nativesharp(
                source.read_text(encoding="utf-8"), source.name, without_comments=True
            )
            sections.extend(
                [
                    f"##### `{source.relative_to(ROOT).as_posix()}`",
                    "",
                    "```csharp",
                    formatted,
                    "```",
                    "",
                ]
            )
    return "\n".join(sections).rstrip()


def fence_for(path: Path) -> str:
    if path.suffix == ".nsproj":
        return "xml"
    if path.suffix == ".ns":
        return "csharp"
    return "text"


def render_cases() -> str:
    sections: list[str] = []
    for number, case in enumerate(CASES, start=1):
        directory = EXAMPLES / case.directory
        project_files = sorted(directory.glob("*.nsproj"))
        source_files = sorted(directory.glob("*.ns"), key=lambda p: (p.name == "Program.ns", p.name.lower()))
        files = project_files + source_files
        source_line_count = sum(len(path.read_text(encoding="utf-8").splitlines()) for path in source_files)
        sections.extend(
            [
                f"### 47.{number} {case.title}",
                "",
                f"**Problema.** {case.problem}",
                "",
                f"**Diseño.** {case.design}",
                "",
                f"**Auditoría propuesta.** {case.audit}",
                "",
                f"**Escala real.** {len(source_files)} archivos NativeSharp, {source_line_count} líneas de fuente, "
                "proyecto completo verificado en `-O0` y `-O2`.",
                "",
                "**Reproducción.**",
                "",
                "```powershell",
                "nsc --emit-project-oop-exe `",
                f"    book_examples/{case.directory}/{project_files[0].name} `",
                "    -O2 `",
                f"    -o {case.directory}.exe",
                f"./{case.directory}.exe",
                "```",
                "",
                "#### Listado completo",
                "",
            ]
        )
        for path in files:
            relative = path.relative_to(ROOT).as_posix()
            raw_content = path.read_text(encoding="utf-8")
            content = (
                format_nativesharp(raw_content, path.name)
                if path.suffix == ".ns"
                else format_project_xml(raw_content, path.name)
            )
            sections.extend(
                [
                    f"##### `{relative}`",
                    "",
                    f"```{fence_for(path)}",
                    content,
                    "```",
                    "",
                ]
            )
    return "\n".join(sections).rstrip()


def strip_comments(lines: list[str]) -> list[str]:
    result: list[str] = []
    in_block = False
    for raw in lines:
        line = raw
        if in_block:
            if "*/" not in line:
                result.append("")
                continue
            line = line.split("*/", 1)[1]
            in_block = False
        while "/*" in line:
            before, after = line.split("/*", 1)
            if "*/" in after:
                line = before + after.split("*/", 1)[1]
            else:
                line = before
                in_block = True
                break
        line = line.split("//", 1)[0]
        result.append(line)
    return result


def public_declarations(path: Path) -> list[str]:
    lines = strip_comments(path.read_text(encoding="utf-8").splitlines())
    declarations: list[str] = []
    index = 0
    while index < len(lines):
        stripped = lines[index].strip()
        if not stripped.startswith("public "):
            index += 1
            continue
        parts = [stripped]
        while not any(token in " ".join(parts) for token in ("{", ";")) and index + 1 < len(lines):
            index += 1
            continuation = lines[index].strip()
            if continuation:
                parts.append(continuation)
        declaration = re.sub(r"\s+", " ", " ".join(parts)).strip()
        if "{" in declaration:
            declaration = declaration.split("{", 1)[0].rstrip()
        declarations.append(declaration)
        index += 1
    return declarations


def render_public_api() -> str:
    sections: list[str] = []
    libraries = sorted(path for path in LIB.iterdir() if path.is_dir())
    for library_number, library in enumerate(libraries, start=1):
        sections.extend([f"## A.{library_number} `{library.name}`", ""])
        for source in sorted(library.glob("*.ns")):
            declarations = public_declarations(source)
            if not declarations:
                continue
            namespace_match = re.search(r"^\s*namespace\s+([^\s{]+)", source.read_text(encoding="utf-8"), re.MULTILINE)
            namespace = namespace_match.group(1) if namespace_match else library.name
            sections.extend([f"### `{namespace}` - `{source.name}`", ""])
            sections.extend(f"- `{declaration}`" for declaration in declarations)
            sections.append("")
    return "\n".join(sections).rstrip()


def render_drozdek_core_source(filename: str, title: str, *, include_project: bool = False) -> str:
    sections = [
        f"#### Implementación completa: {title}",
        "",
        "Este listado es parte del capítulo: contiene la estructura y el programa de ejercicio correspondiente. "
        "No es pseudocódigo ni un fragmento recortado.",
        "",
    ]
    if include_project:
        project = DROZDEK_CORE / "Book27DrozdekCoreStructures.nsproj"
        sections.extend(
            [
                f"##### `{project.relative_to(ROOT).as_posix()}`",
                "",
                "```xml",
                format_project_xml(project.read_text(encoding="utf-8"), project.name),
                "```",
                "",
            ]
        )
    source = DROZDEK_CORE / filename
    sections.extend(
        [
            f"##### `{source.relative_to(ROOT).as_posix()}`",
            "",
            "```csharp",
            format_nativesharp(source.read_text(encoding="utf-8"), source.name),
            "```",
        ]
    )
    return "\n".join(sections)


def render_drozdek() -> str:
    manuscript = DROZDEK.read_text(encoding="utf-8").rstrip()
    listings = [
        ("DROZDEK_LINKED_LIST", "LinkedListExample.ns", "lista simplemente enlazada", True),
        ("DROZDEK_STACK", "StackExample.ns", "pila dinámica", False),
        ("DROZDEK_QUEUE", "QueueExample.ns", "cola circular", False),
        ("DROZDEK_BINARY_TREE", "BinaryTreeExample.ns", "árbol binario general", False),
        ("DROZDEK_BST", "BinarySearchTreeExample.ns", "árbol binario de búsqueda", False),
        ("DROZDEK_AVL", "AvlExample.ns", "árbol AVL", False),
        ("DROZDEK_GRAPH", "GraphExample.ns", "grafo ponderado", False),
        ("DROZDEK_HASH", "HashTableExample.ns", "tabla hash con direccionamiento abierto", False),
    ]
    for marker, filename, title, include_project in listings:
        manuscript = manuscript.replace(
            f"<!-- GENERATED:{marker} -->",
            render_drozdek_core_source(filename, title, include_project=include_project),
        )
    return manuscript


def build() -> None:
    manuscript = BASE.read_text(encoding="utf-8")
    manuscript = manuscript.replace("<!-- GENERATED:TUTORIALS -->", render_tutorials())
    manuscript = manuscript.replace("<!-- GENERATED:BOOK_EXAMPLES -->", render_cases())
    manuscript = manuscript.replace("<!-- GENERATED:DROZDEK -->", render_drozdek())
    manuscript = manuscript.replace("<!-- GENERATED:DEITEL -->", DEITEL.read_text(encoding="utf-8").rstrip())
    manuscript = manuscript.replace("<!-- GENERATED:PUBLIC_API -->", render_public_api())
    OUTPUT.write_text(manuscript, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    build()
