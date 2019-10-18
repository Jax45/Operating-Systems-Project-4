#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int openBit(unsigned char * bitMap, int lastPid){
        lastPid++;
        if(lastPid > 17){
          lastPid = 0;
        }
        int tempPid = lastPid;
            while(1){
                if((bitMap[lastPid/8] & (1 << (lastPid % 8 ))) == 0){
                        /*bit has not been set yet*/
                        return lastPid;
                }

                lastPid++;
                if(lastPid > 17){
                        lastPid = 0;
                }
                if( lastPid == tempPid){
                        return -1;
                }

            }
}

void setBit(unsigned char * bitMap, int lastPid){
        bitMap[lastPid/8] |= (1<<(lastPid%8));
}

void resetBit(unsigned char * bitMap, int finPid){
        bitMap[finPid/8] &= ~(1 << (finPid % 8));

}
