/*
 * flash.c
 *
 *  Created on: Oct 27, 2011
 *      Author: Alexander Khoryakov
 */

#include "mishka_declaration.h"
#include "../stm32fx/inc/stm32f413xx.h"

const Koefs* flashKoefs = 	(Koefs*)(FLASH_BASE+PAGE_SIZE*(MAX_PAGE_NUMBER-1)); 

int16_t correctKoefsCopy=PAGE_SIZE/sizeof(Koefs)-1; //start from end

uint32_t getKoefsCRC(Koefs* src){

	uint32_t crc = 0x1983, i;
	uint8_t* addr = (uint8_t*)src;  // Fix: Cast to uint8_t* instead of uint32_t
	for (i = 0; i < (sizeof(Koefs) - 4); i++){
		crc += *(addr++);
	}
	return crc;
}

void setKoefsDefault(Koefs * koefs){
	koefs->steerRatio = 43;
	koefs->steerActuatorDelay = 210;
}

void writeFlash(void* Src, void* Dst, int Len){
  uint16_t* SrcW = (uint16_t*)Src;
  uint16_t* DstW = (uint16_t*)Dst;


  /* (1) Wait till no operation is on going */
  /* (2) Check that the Flash is unlocked */
  /* (3) Perform unlock sequence */
  while ((FLASH->SR &FLASH_SR_BSY) != 0);
  if ((FLASH->CR &FLASH_CR_LOCK) != 0){
	  FLASH->KEYR = FLASH_KEY1;
	  FLASH->KEYR = FLASH_KEY2;
  }

  FLASH->CR |= FLASH_CR_PG; /* Program the flash */


while (Len > 0){
//	if (SrcW%2)
//		DEBUG_MSG(BDL,("WARNING odd address!!!"))

    *DstW = *SrcW;
    while ((FLASH->SR & FLASH_SR_BSY) != 0 );

    if (*DstW != *SrcW ){
      puts(COL_RED"Wrong write "COL_END); puth(*DstW ); puts("  "); puth(*SrcW ); puts("\n\r");
      goto EndPrg;
    }

    DstW++;
    SrcW++;
    Len-=2;
}


EndPrg:


if ((FLASH->SR &FLASH_SR_EOP) != 0){
	FLASH->SR =FLASH_SR_EOP;
}else{
	/* Manage the error cases */
}

  FLASH->CR &= ~FLASH_CR_PG; /* Reset the flag back !!!! */
//Lock the flash back
  FLASH->CR |= FLASH_CR_LOCK;
}

// Function to get sector number from address for STM32F413
uint8_t getSectorNumber(uint32_t addr) {
    // STM32F413 has 8 sectors of 128KB each (total 1MB)
    // Sector 0: 0x08000000 - 0x0801FFFF (128KB)
    // Sector 1: 0x08020000 - 0x0803FFFF (128KB)
    // Sector 2: 0x08040000 - 0x0805FFFF (128KB)
    // Sector 3: 0x08060000 - 0x0807FFFF (128KB)
    // Sector 4: 0x08080000 - 0x0809FFFF (128KB)
    // Sector 5: 0x080A0000 - 0x080BFFFF (128KB)
    // Sector 6: 0x080C0000 - 0x080DFFFF (128KB)
    // Sector 7: 0x080E0000 - 0x080FFFFF (128KB)
    
    if (addr < FLASH_BASE || addr >= (FLASH_BASE + 0x180000)) {
        return 0xFF; // Invalid address
    }
    
    uint32_t offset = addr - FLASH_BASE;
    return (uint8_t)(offset / PAGE_SIZE); // Each sector is 128KB (0x20000 bytes)
}

void eraseSector(void* addr)
{
    uint8_t sector = getSectorNumber((uint32_t)addr);
    
    puts(COL_GREEN"Erace sector "COL_END); puth(sector);puts("\n\r");

    if (sector == 0xFF) {
        // Invalid address, return error
        return;
    }

return; //temp, instead USB doesn't work

      // Unlock flash
    if ((FLASH->CR & FLASH_CR_LOCK) != 0) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    
    // Wait for any ongoing operation to complete
    while ((FLASH->SR & FLASH_SR_BSY) != 0);
    
    // Clear any error flags
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_EOP | FLASH_SR_WRPERR | 
                FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;
    
    // Set sector erase bit
    FLASH->CR |= FLASH_CR_SER;
    
    // Clear sector number field and set the sector to erase
    FLASH->CR &= ~FLASH_CR_SNB;
    FLASH->CR |= (sector << FLASH_CR_SNB_Pos) & FLASH_CR_SNB;
    
    // Start erase operation
    FLASH->CR |= FLASH_CR_STRT;
    
    // Wait for operation to complete
    while ((FLASH->SR & FLASH_SR_BSY) != 0);
    
    // Check for errors
    if (FLASH->SR & (FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR | 
                     FLASH_SR_PGPERR | FLASH_SR_PGSERR)) {

        puts(COL_GREEN"Erace errors "COL_END); puth(FLASH->SR);puts("\n\r");
        // Handle error - clear error flags
        FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR | 
                    FLASH_SR_PGPERR | FLASH_SR_PGSERR;
    }
    
    // Clear sector erase bit
    FLASH->CR &= ~FLASH_CR_SER;
    
    // Lock the flash back
    FLASH->CR |= FLASH_CR_LOCK;
}


//save koefs in eeprom
void setKoefs(Koefs * koefs){

koefs->crc = getKoefsCRC(koefs);

	if ((correctKoefsCopy >= (uint16_t)(PAGE_SIZE/sizeof(Koefs)-1))){ //end space on page
		correctKoefsCopy=0;
		eraseSector((void*)flashKoefs);
		writeFlash((void*)koefs, (void*)flashKoefs, sizeof(Koefs));
	}else{ //write
		correctKoefsCopy++;
		writeFlash((void*)koefs, (void*)(flashKoefs+correctKoefsCopy), sizeof(Koefs));
	}
}

//get koefs from eeprom
uint8_t getKoefs(Koefs * koefs){

uint16_t i;
uint16_t* SrcW = (uint16_t*)flashKoefs;
uint16_t* DstW = (uint16_t*)koefs;

//looking for right copy
		while (correctKoefsCopy > -1){
			SrcW = (uint16_t*)flashKoefs + correctKoefsCopy*sizeof(Koefs)/2;
			for(i=0;i<sizeof(Koefs)/2;i++)
				*(DstW+i) = *(SrcW+i);

      //if (koefs->crc != 0xffffffff)
      //  puts(COL_GREEN"Check CRC "COL_END); puth(koefs->crc);puts(" "); puth(getKoefsCRC(koefs)); puts("\n\r");

			if (koefs->crc == getKoefsCRC(koefs)) //copy is OK
				return 1;
			else{ 
				correctKoefsCopy--; //try next copy
			}
		};

    puts(COL_GREEN"Set koefs default ..."COL_END); puts("\n\r");
		setKoefsDefault(koefs);

		correctKoefsCopy = PAGE_SIZE/sizeof(Koefs);//erase pages & write new values
		setKoefs(koefs);
	return 0;
}


uint32_t getCRC(uint8_t *data, uint32_t size){
	uint32_t crc = 0, i;
	for (i = 0; i < size; i++){
		//DEBUG_MSG(BDL,("add crc %x at %d", *data, i))
		crc += *(data++);
	}
	return crc;
}




