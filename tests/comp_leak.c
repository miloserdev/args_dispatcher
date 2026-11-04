#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ARGS_REPL
#include "args.h"
#include "commands/chassis.h"
#include "commands/info.h"
#include "commands/ip.h"
#include "commands/power.h"
args_command_t cmd_table[] = { chassis, power, info, ip, NULL };
static args_context_repl_t repl = { .root = cmd_table };
static args_argv_t av;
static args_context_t ctx = { .av = &av, .repl = &repl };
int main(void){
  const char *lines[] = {"","c","i","ip ","ip a","ip addr ","ip addr add 1.1.1.1/24 ","ip addr add 1.1.1.1/24 d","ip addr add 1.1.1.1/24 dev ","ip addr add 1.1.1.1/24 dev eth0 l","ip route add default ","ip route add default v","ip link set eth0 ","ip link set eth0 up ","ip neigh add 1.1.1.1 ","po","power ","info ","info set a b ","zzz ","ip addr add 1.1.1.1/24 dev eth0 label x broadcast 1.1.1.255 ",NULL};
  for(int i=0;lines[i];i++){
    const char *l=lines[i]; int n=strlen(l); int st=n; while(st>0&&l[st-1]!=' ')st--;
    char **m=args_complete(&ctx,l,l+st,st);
    if(m){ for(int j=1;m[j];j++)free(m[j]); free(m[0]); free(m);}   /* readline frees like this */
  }
  puts("ok"); return 0; }
