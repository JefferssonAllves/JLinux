#include "super-tap.h"
void super_tap_press(struct super_tap *tap,uint32_t now){
    if(!tap->down){tap->down=true;tap->pressed=now;}
}
bool super_tap_release(struct super_tap *tap,uint32_t now){
    if(!tap->down){return false;}tap->down=false;
    if((uint32_t)(now-tap->pressed)>250){tap->first=false;return false;}
    if(tap->first&&(uint32_t)(tap->pressed-tap->released)<=350){tap->first=false;return true;}
    tap->first=true;tap->released=now;return false;
}
void super_tap_cancel(struct super_tap *tap){*tap=(struct super_tap){0};}
