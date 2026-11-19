#include "../src/shell/bluetooth.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Falha: %s\n",#x);return 1;}}while(0)
int main(void){
    const char *values[]={"b true\n","b false\n","b truejunk\n","","Adapter unavailable\n"};
    for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++){
        int pipes[2];CHECK(pipe(pipes)==0);CHECK(write(pipes[1],values[i],strlen(values[i]))==(ssize_t)strlen(values[i]));close(pipes[1]);
        struct bluetooth bt={.fd=pipes[0],.action=-1,.pid=123};bluetooth_complete(&bt,0);
        CHECK(bt.available==(i<2));if(i<2){CHECK(bt.powered==(i==0));}CHECK(!bt.pid && bt.fd==-1);
    }
    int pipes[2];CHECK(pipe(pipes)==0);close(pipes[1]);struct bluetooth bt={.fd=pipes[0],.action=1,.available=true,.powered=false};
    bluetooth_complete(&bt,256);CHECK(bt.available && !bt.powered && bt.error[0]);
    CHECK(!bluetooth_start(&bt,2));return 0;
}
