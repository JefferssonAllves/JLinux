#include "../src/compositor/super-tap.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Falha: %s\n",#x); return 1; } } while(0)
int main(void){
 struct super_tap tap={0};
 super_tap_press(&tap,100);CHECK(!super_tap_release(&tap,140));
 super_tap_press(&tap,250);CHECK(super_tap_release(&tap,290));
 super_tap_press(&tap,350);CHECK(!super_tap_release(&tap,370));
 super_tap_press(&tap,900);CHECK(!super_tap_release(&tap,920));
 super_tap_cancel(&tap);
 super_tap_press(&tap,1000);super_tap_press(&tap,1300);CHECK(!super_tap_release(&tap,1400));
 super_tap_press(&tap,1450);CHECK(!super_tap_release(&tap,1500));
 super_tap_press(&tap,1550);super_tap_cancel(&tap);CHECK(!super_tap_release(&tap,1570));
 super_tap_press(&tap,1600);CHECK(!super_tap_release(&tap,1630));
 super_tap_cancel(&tap);
 super_tap_press(&tap,UINT32_MAX-100);CHECK(!super_tap_release(&tap,UINT32_MAX-80));
 super_tap_press(&tap,20);CHECK(super_tap_release(&tap,40));
 return 0;
}
