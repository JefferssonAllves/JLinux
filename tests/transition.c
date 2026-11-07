#include "../src/animation/transition.h"
#include <math.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Falha: %s\n",#x); return 1; } } while(0)
int main(void) {
    struct transition t={0};
    transition_target(&t,1,10,.2);
    CHECK(transition_value(&t,10)==0 && transition_active(&t,10));
    CHECK(fabs(transition_value(&t,10.1)-.5)<1e-8);
    double before=transition_value(&t,10.08);
    transition_target(&t,0,10.08,.16);
    CHECK(transition_value(&t,10.08)==before);
    CHECK(transition_value(&t,10.5)==0 && !transition_active(&t,10.5));
    transition_target(&t,1,11,0);
    CHECK(transition_value(&t,11)==1 && !transition_active(&t,11));
    transition_target(&t,0,12,.16);
    for(int i=0;i<=100;i++){double v=transition_value(&t,12+i*.002);CHECK(v>=0 && v<=1);}
    return 0;
}
