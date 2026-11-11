#include "api.h"
int main(void) { unsigned char b=10; short s=20; int value=1; MostPtr mp=t09_get_most(); MostPtr md=t09_most;
MostVoid mv=t09_get_most_void(); MostVoid mvd=t09_most_void; AllPtr ap=t09_get_all_ptr(); AllPtr ad=t09_all_ptr;
AllVoid av=t09_get_all(); AllVoid avd=t09_all; unsigned char* p1=t09_most(&b,1); unsigned char* p2=mp(&b,1);
t09_most_void((void*)&value,1); mv((void*)&value,1); short* p3=t09_all_ptr(&s,1); short* p4=ap(&s,1);
t09_all((void*)&value,1); av((void*)&value,1);
return mp==md && mv==mvd && ap==ad && av==avd && p1==&b && p2==&b && p3==&s && p4==&s && b==12 && s==22 && value==5 ? 0 : 1; }
