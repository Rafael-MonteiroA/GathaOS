#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libgatha/tls/x509.h"
static uint32_t rng=0x12345678;
static unsigned random_word(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(int argc,char **argv) {
    if(argc!=2)return 1;FILE *f=fopen(argv[1],"rb");if(!f)return 1;uint8_t original[16384],data[16384];unsigned n=(unsigned)fread(original,1,sizeof(original),f);fclose(f);if(!n)return 1;
    for(unsigned i=0;i<30000;++i){memcpy(data,original,n);unsigned count=1+random_word()%8;while(count--)data[random_word()%n]^=(uint8_t)(1+random_word()%255);unsigned size=i%3?n:random_word()%(n+1);struct certificate cert;if(!x509_parse((struct der){data,size},&cert))x509_name(&cert,"one.unit.test");}
    for(unsigned nbytes=0;nbytes<=n;++nbytes){struct certificate cert;x509_parse((struct der){original,nbytes},&cert);}
    puts("X509 sanitizer mutations PASS");return 0;
}
