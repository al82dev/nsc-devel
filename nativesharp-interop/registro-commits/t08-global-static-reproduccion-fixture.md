# T08-004: control de global C estatica y funciones inline

El control C y el consumidor NativeSharp escriben y leen una variable interna
directamente y mediante funciones inline del mismo encabezado. Este commit
prepara la reproduccion; no afirma que NSC la compile ni que el control C
se haya ejecutado. El ejecutable C requiere enlazado y ejecucion separados.
