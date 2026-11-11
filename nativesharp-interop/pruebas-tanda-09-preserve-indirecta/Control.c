#include "api.h"
int main(void) { T09MostCallback m=t09_get_most(), dm=t09_most; T09AllCallback a=t09_get_all(), da=t09_all;
return m==dm && a==da && t09_most(3,4,7)==347 && t09_all(3,4,7)==3047 && m(3,4,7)==347 && a(3,4,7)==3047 ? 0 : 1; }
