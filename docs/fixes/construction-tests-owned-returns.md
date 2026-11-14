# Pruebas de resolución de constructores y retorno propietario

Cuatro pruebas de resolución devolvían un objeto recién creado desde un método con retorno
sin `owned`. El análisis de ownership las detenía antes de verificar la aridad, accesibilidad
o ambigüedad de constructor que las pruebas pretendían observar.

Los cuatro métodos de los fixtures ahora devuelven `owned`; sus aserciones de diagnóstico
permanecen intactas. El primer ejemplo original se conserva en un control separado que verifica
el diagnóstico previo de pérdida de propietario. No se modifica código de producto.

Antes del ajuste, la prueba dirigida fallaba en cuatro de 73 comprobaciones. Una ejecución
instrumentada confirmó el diagnóstico de pérdida de propietario en las cuatro. Después del
ajuste y del control adicional, el blanco `nsc_project_construction_model_tests` compiló
incrementalmente y el ejecutable terminó con `PASS: 74 checks`. La instrumentación temporal
se retiró antes de esta ejecución y no forma parte del cambio.
