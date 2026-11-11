#include "api.h"
int main(void) { T09Record record; record.x=10; record.y=20; int value=1; int* p=&value;
MostPtr mp=t09_get_most(); MostPtr md=t09_most; MostVoid mv=t09_get_most_void(); MostVoid mvd=t09_most_void;
AllPtr ap=t09_get_all_ptr(); AllPtr ad=t09_all_ptr; AllVoid av=t09_get_all(); AllVoid avd=t09_all;
T09Record* r1=t09_most(&record,1); T09Record* r2=mp(&record,1); t09_most_void(&p,1); mv(&p,1);
int** p1=t09_all_ptr(&p,1); int** p2=ap(&p,1); t09_all(&record,1); av(&record,1);
return mp==md && mv==mvd && ap==ad && av==avd && r1==&record && r2==&record && p1==&p && p2==&p && record.x==12 && record.y==22 && value==5 ? 0 : 1; }
