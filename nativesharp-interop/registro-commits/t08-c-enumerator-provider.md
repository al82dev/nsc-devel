# Tanda 08: proveedor inicial de enumeradores C

El proveedor busca los nombres solicitados en los proyectos visibles, conserva
el tipo y el valor convertidos y prepara un estado constante sin símbolo externo.
Observaciones incompatibles o no representables producen un error diferido en
el candidato. Esta ruta cubre enums C globales: falta integrarla con la selección
de variables y otros ámbitos, y demostrar expresiones constantes y generación.
