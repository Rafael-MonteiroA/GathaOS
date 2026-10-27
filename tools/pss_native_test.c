#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libgatha/crypto/crypto.h"
static uint32_t rng=0x73982715;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(int argc,char **argv) {
    if(argc!=2)return 1;FILE *file=fopen(argv[1],"rb");if(!file)return 1;
    uint32_t count;if(fread(&count,4,1,file)!=1)return 1;
    struct rsa_key key;uint8_t digest[64],signature[512];unsigned positives=0;
    for(unsigned i=0;i<count;++i){
        uint32_t fields[6];
        if(fread(fields,4,6,file)!=6 || fread(key.modulus,1,512,file)!=512 ||
           fread(digest,1,64,file)!=64 || fread(signature,1,512,file)!=512)return 1;
        key.size=fields[0];key.exponent=fields[1];
        if((rsa_pss_verify(&key,fields[2],fields[3],digest,signature,fields[4])==0)!=fields[5])return 2;
        positives+=fields[5];
    }
    fclose(file);
    for(unsigned i=0;i<2000;++i){
        uint8_t mutated[512];memcpy(mutated,signature,sizeof(mutated));
        for(unsigned j=0;j<1+i%8;++j)mutated[next()%512]^=(uint8_t)next();
        rsa_pss_verify(&key,3+next()%5,next()%550,digest,mutated,next()%513);
    }
    printf("PSS SANITIZER PASS: %u vectors (%u positive), 2000 mutations\n",count,positives);
    return 0;
}
