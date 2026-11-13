#include "api.h"
int main() {
 T09Cpp::Small s{10,20,30,40,50,60}; T09Cpp::Other t{100,200,300,400,500};
 auto callback=T09Cpp::fetchCallback(); int value=7; int* inner=&value; int** a=T09Cpp::most(s,&inner,t); int** b=callback(s,&inner,t); bool ok=a==&inner && b==a && *a==inner && **b==627; **a=55; ok=ok && value==55 && **b==55;
 return callback==&T09Cpp::most && s.x==10 && s.y==20 && s.z==30 && s.w==40 && s.v==50 && s.u==60 &&
 t.x==100 && t.y==200 && t.z==300 && t.w==400 && t.v==500 && ok ? 0 : 1;
}