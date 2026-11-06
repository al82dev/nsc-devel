# T08-015: comparación volatile mediante puntero

El caso previo comparó C con acceso global contra NativeSharp con acceso por
puntero. Se añade una función C que usa el mismo tipo de receptor para aislar
esa diferencia al comparar su IR. El resultado requiere inspección directa.
