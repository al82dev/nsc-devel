#include "api.h"
int main(void) {
  if (t09_regcall(-7)!=-6 || t09_uc(250)!=251 || t09_sh(-1000)!=-999 || t09_us(65000)!=65001) return 1;
  T09SC a=t09_get_sc(); T09UC b=t09_get_uc(); T09SH c=t09_get_sh(); T09US d=t09_get_us();
  return a(-7)==-6 && b(250)==251 && c(-1000)==-999 && d(65000)==65001 ? 0 : 2;
}
