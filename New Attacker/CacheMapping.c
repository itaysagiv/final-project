#include "CacheFuncs.h"
#include <stdio.h>

int main() {

Cache_Mapping EvicSets;
LinesBuffer Buff;

fprintf(stderr,"Starting\n",30);
CreateEvictionSet(Buff.Lines->Bytes,sizeof(Buff),&EvicSets);

return 0;
}
