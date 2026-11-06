# Comparación numérica de enums C importados

El análisis reconoce la representación de enums C completos al compararlos
con números; HIR selecciona el tipo numérico común de NativeSharp e inserta
conversiones desde la identidad enum. Los enums nativos conservan sus reglas y
no se amplían conversiones de asignación. Faltan controles de signos y anchuras
y validación agrupada de semántica y generación LLVM.
