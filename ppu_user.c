#include "stdio.h"
#include "stdint.h"
#include <poll.h>
#define PPU_DEV "/dev/lkmc_ppu"
#define PPU_MODE 8
#define PPU_STATUS 0x24
#define PPU_R0  12
#define PPU_DMA_IRQ 0x4
#define PPU_DMA_SRC 0x84
#define PPU_DMA_DST 0x8c
#define PPU_DMA_CNT 0x94
#define PPU_DMA_CMD 0x9c
#define PPU_DMA_RBACK 0xF4

typedef FILE* PPU_PTR;

uint32_t check_dma(PPU_PTR ppu) {
    uint32_t fp32 = 0x1; //0.5
    fseek(ppu, PPU_DMA_CMD, SEEK_SET);
    fread(&fp32,4,1,ppu);
    return fp32 == PPU_DMA_IRQ;
}

uint32_t check_status(PPU_PTR ppu) {
    uint32_t fp32 = 0x1; //0.5
    fseek(ppu, PPU_STATUS, SEEK_SET);
    fread(&fp32,4,1,ppu);
    return !fp32;
}

uint8_t float_to_p8(float f,PPU_PTR ppu) {
    uint8_t p8 = -1;
    uint32_t mode = 0x00000011;
    fseek(ppu, PPU_MODE, SEEK_SET );  
    fwrite(&mode,1,sizeof(mode),ppu);
    fflush(ppu); 
    fseek(ppu, PPU_R0, SEEK_SET);
    fwrite(&f, 4,1,ppu);
    while(!check_status(ppu)) {}; // spin lock, bleah
    fread(&p8,1,1,ppu);
    return p8;
}

uint8_t float_to_p16(float f,PPU_PTR ppu) {
    uint16_t p16 = -1;
    uint32_t mode = 0x00000012;
    fseek(ppu, PPU_MODE, SEEK_SET );  
    fwrite(&mode,1,sizeof(mode),ppu);
    fflush(ppu); 
    fseek(ppu, PPU_R0, SEEK_SET);
    fwrite(&f, 4,1,ppu);
    while(!check_status(ppu)) {}; // spin lock, bleah
    fread(&p16,1,2,ppu);
    return p16;
}


void wfdma(PPU_PTR ppu) {
    while(!check_dma(ppu)) {};
}
// 0x5
// 0b101

// 0x7
// 0b111

int main() {
    PPU_PTR fp;
    float f = 0.5;
    uint32_t i = 0xffffffff, cnt = 4, cmd = 0x5, cmdin = 0x7, dst = 0x40000;
    float *ffp = &f;
    uint32_t* ip = &i, *dstp = &dst;
    fp = fopen(PPU_DEV,"r+");
    printf("ID file:%d\n",fp);

    uint8_t p8 = float_to_p8(f,fp);
    uint16_t p16 = float_to_p16(f,fp);


    fseek(fp, PPU_DMA_SRC, SEEK_SET );  
    fwrite(&ip,1,sizeof(ip),fp);
    fflush(fp); 



    fseek(fp, PPU_DMA_CNT, SEEK_SET );  
    fwrite(&cnt,1,sizeof(cnt),fp);
    fflush(fp);         

    fseek(fp, PPU_DMA_CMD, SEEK_SET );  
    fwrite(&cmd,1,sizeof(cmd),fp);
    fflush(fp);   
    //wfdma(fp);   

    struct pollfd pfd;
    pfd.fd = fileno(fp);
    pfd.events = POLLIN;
    poll(&pfd,1,-1);


    printf("Destination buffer: %llx\n",dstp);
    fseek(fp, PPU_DMA_DST, SEEK_SET );  
    fwrite(&dstp,1,sizeof(dstp),fp);
    fflush(fp);

    fseek(fp, PPU_DMA_CMD, SEEK_SET );  
    fwrite(&cmdin,1,sizeof(cmdin),fp);
    fflush(fp);

    struct pollfd pfd2;
    pfd2.fd = fileno(fp);
    pfd2.events = POLLIN;           
    poll(&pfd2,1,-1);

    fseek(fp, PPU_DMA_RBACK, SEEK_SET );  
    fread(&dst,4,4,fp);


    printf("Received DMA response: %lx\n",*dstp);

    fclose(fp);
  
   return(0); 
}