#include "api.h"
int main(void) { T09Small s; s.x=10; s.y=20; T09Large l; l.a=1; l.b=2; l.c=3; l.d=4; l.e=5; l.f=6;
MostPtr mp=t09_get_most(); MostVoid mv=t09_get_most_void(); AllPtr ap=t09_get_all_ptr(); AllVoid av=t09_get_all();
T09Small s1=t09_most(s); T09Small s2=mp(s); T09Large l1=t09_most_void(l); T09Large l2=mv(l);
T09Small s3=t09_all_ptr(s); T09Small s4=ap(s); T09Large l3=t09_all(l); T09Large l4=av(l);
return s.x==10 && s.y==20 && l.a==1 && l.f==6 && s1.x==11 && s2.x==11 && s3.x==13 && s4.x==13 && s1.y==20 && s4.y==20 && l1.a==3 && l2.a==3 && l3.a==5 && l4.a==5 && l1.f==6 && l4.f==6 ? 0 : 1; }
