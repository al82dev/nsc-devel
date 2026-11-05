# Entorno de sesión en el conductor

El conductor prepara la fábrica de Clang y el entorno de sesión cuando se
solicita una raíz de Clang. Rechaza inclusiones sin raíz y comunica cuando la
integración está desactivada al construir el compilador.

El ámbito de ejecución conserva la configuración y una referencia a la fábrica
durante la ejecución sincrónica. Este commit registra cambios acumulados; su
validación corresponde al conjunto de la tanda.
