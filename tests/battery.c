#define _GNU_SOURCE
#include "../src/shell/battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Falha: %s\n",#x); return 1; } } while(0)
static void write_file(const char *path, const char *text) { FILE *file = fopen(path,"w"); if (!file) { abort(); } fputs(text,file); fclose(file); }
int main(void) {
    char root[] = "/tmp/jlinux-battery-XXXXXX"; CHECK(mkdtemp(root));
    char folder[256], capacity[256], status[256];
    snprintf(folder,sizeof(folder),"%s/BAT0",root); CHECK(!mkdir(folder,0700));
    snprintf(capacity,sizeof(capacity),"%s/BAT0/capacity",root);
    snprintf(status,sizeof(status),"%s/BAT0/status",root);
    CHECK(battery_read(root).percent == -1);
    write_file(capacity,"75\n"); write_file(status,"Charging\n");
    struct battery_status value = battery_read(root); CHECK(value.percent == 75 && value.charging);
    const char *states[] = {"Discharging\n","Full\n","Not charging\n","Unknown\n"};
    for (size_t i=0;i<4;i++) { write_file(status,states[i]); value=battery_read(root); CHECK(value.percent==75 && !value.charging); }
    CHECK(!unlink(status)); CHECK(!battery_read(root).charging);
    const char *invalid[] = {"-1\n","101\n","abc\n","999999999999999999999999\n","75garbage\n"};
    for(size_t i=0;i<5;i++){write_file(capacity,invalid[i]);CHECK(battery_read(root).percent == -1);}
    CHECK(!unlink(capacity)); CHECK(!rmdir(folder)); CHECK(!rmdir(root)); return 0;
}
