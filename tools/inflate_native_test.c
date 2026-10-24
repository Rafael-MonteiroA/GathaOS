/* Host-only harness: the decoder is the exact source linked into the guest. */
#include "../libgatha/inflate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static unsigned calls,cancel_at;
static int cancel(void *context){++calls;return calls>=cancel_at?*(int *)context:0;}
static unsigned word(FILE *file) {
    uint8_t b[4];assert(fread(b,1,4,file)==4);
    return (unsigned)b[0]|(unsigned)b[1]<<8|(unsigned)b[2]<<16|(unsigned)b[3]<<24;
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);
    unsigned count=word(file);
    for(unsigned i=0;i<count;++i) {
        unsigned encoding=word(file),capacity=word(file),n=word(file),expect=word(file),size=word(file);
        uint8_t *input=malloc(n+1),*output=malloc(capacity+32),*expected=malloc(size+1);assert(input&&output&&expected);
        assert(fread(input,1,n,file)==n);assert(fread(expected,1,size,file)==size);
        memset(output,0xa7,capacity+32);unsigned written=123;
        int r=g_inflate(input,n,output+16,capacity,&written,encoding,0,0);
        if((expect==0 && (r || written!=size || memcmp(output+16,expected,size))) ||
           (expect==1 && (r>=0 || written)) || (expect>1 && (r!=-(int)expect || written))) {
            fprintf(stderr,"inflate vector %u: result %d size %u, expected %u size %u\n",i,r,written,expect,size);abort();
        }
        for(unsigned j=0;j<16;++j)assert(output[j]==0xa7&&output[16+capacity+j]==0xa7);
        if(!expect && size+n>32768) {
            for(cancel_at=1;cancel_at<=2;++cancel_at) {
                int error=cancel_at==1?-125:-110;calls=0;written=123;
                assert(g_inflate(input,n,output+16,capacity,&written,encoding,cancel,&error)==error);
                assert(!written && calls==cancel_at);
            }
        }
        free(input);free(output);free(expected);
    }
    assert(fgetc(file)==EOF);fclose(file);
    printf("INFLATE NATIVE PASS: %u independent vectors/mutations, bounded output, checksums and cancellation under ASan/UBSan\n",count);
    return 0;
}
