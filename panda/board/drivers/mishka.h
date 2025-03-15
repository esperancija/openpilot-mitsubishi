
void DMA2_Stream0_IRQh(void){
static uint32_t i;
    // Check for transfer complete interrupt
    if (DMA2->LISR & DMA_LISR_TCIF0){
    	if ((i++) % 2)
    		set_gpio_output(GPIOC, 12, false);
    	else
    		set_gpio_output(GPIOC, 12, true);

//        puts("NDTR\n");
//        puth(DMA2_Stream0->NDTR);

        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;  // Clear transfer complete flag
    }
}

void TIM3_IRQh(void){

//static uint32_t i;
//
//	if ((i++) % 2)
//		set_gpio_output(GPIOC, 12, false);
//	else
//		set_gpio_output(GPIOC, 12, true);

	if (TIM3->SR & TIM_SR_UIF){
		if (ADC1->SR & ADC_SR_OVR){
			ADC1->SR &= ~(ADC_SR_OVR);
			puts("ADC_OVR");
		}

		//ADC1->CR2 |= ADC_CR2_SWSTART;

		TIM3->SR &= ~TIM_SR_UIF;
	}
}

void ADC_IRQh(void){
	if (ADC1->SR & ADC_SR_EOC){
        //set_gpio_mode(GPIOB, 4, MODE_OUTPUT);
        //GPIOB->ODR ^= GPIO_ODR_ODR_4;

        puts("\n");
        puth(ADC1->DR);
        puts("\n");
//        puth(DMA2_Stream0->NDTR);
//		puts("\n");

		ADC1->SR &= ~(ADC_SR_EOC | ADC_SR_OVR);
	}
}

// ***************************** main code *****************************

#define TENZO1_ADC_CH	1
#define TENZO2_ADC_CH	6
#define SBI_ADC_CH		7

uint16_t rawAdcData[DMA_NUM_CH];
void mishka_init(void){

//	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
//	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	 set_gpio_mode(GPIOA, TENZO1_ADC_CH, MODE_ANALOG);
	 set_gpio_mode(GPIOA, TENZO2_ADC_CH, MODE_ANALOG);
	 set_gpio_mode(GPIOA, SBI_ADC_CH, MODE_ANALOG);

    register_set(&(ADC1->CR1), ADC_CR1_SCAN | ADC_CR1_EOCIE,
    									ADC_CR1_SCAN | ADC_CR1_EOCIE);

    register_set(&(ADC1->SQR1),
    		((DMA_NUM_CH-1) << ADC_SQR1_L_Pos) |
    		(TENZO1_ADC_CH << ADC_SQR1_SQ16_Pos)  |
			(TENZO2_ADC_CH << ADC_SQR1_SQ15_Pos)  |
			(TENZO1_ADC_CH << ADC_SQR1_SQ14_Pos)  |
			(TENZO2_ADC_CH << ADC_SQR1_SQ13_Pos)
			,ADC_SQR1_L | ADC_SQR1_SQ16_Msk | ADC_SQR1_SQ15_Msk | ADC_SQR1_SQ14_Msk | ADC_SQR1_SQ13_Msk);

	register_set(&(ADC1->SQR2),
			TENZO1_ADC_CH << ADC_SQR2_SQ12_Pos  |
			TENZO1_ADC_CH << ADC_SQR2_SQ11_Pos  |
			TENZO1_ADC_CH << ADC_SQR2_SQ10_Pos  |
				TENZO2_ADC_CH << ADC_SQR2_SQ9_Pos  |
				TENZO2_ADC_CH << ADC_SQR2_SQ8_Pos  |
			TENZO2_ADC_CH << ADC_SQR2_SQ7_Pos,
			ADC_SQR2_SQ12_Msk | ADC_SQR2_SQ11_Msk | ADC_SQR2_SQ10_Msk |
				ADC_SQR2_SQ9_Msk | ADC_SQR2_SQ8_Msk | ADC_SQR2_SQ7_Msk);

	register_set(&(ADC1->SQR3),
			TENZO2_ADC_CH << ADC_SQR3_SQ6_Pos  |
			TENZO2_ADC_CH << ADC_SQR3_SQ5_Pos  |
			TENZO2_ADC_CH << ADC_SQR3_SQ4_Pos  |
				TENZO2_ADC_CH << ADC_SQR3_SQ3_Pos  |
				SBI_ADC_CH << ADC_SQR3_SQ2_Pos  |
				SBI_ADC_CH << ADC_SQR3_SQ1_Pos,
			ADC_SQR3_SQ6_Msk | ADC_SQR3_SQ5_Msk | ADC_SQR3_SQ4_Msk |
				ADC_SQR3_SQ3_Msk | ADC_SQR3_SQ2_Msk | ADC_SQR3_SQ1_Msk);


    register_set(&(ADC1->CR2),  ADC_CR2_EXTEN_0 |  ADC_CR2_EXTSEL_3 | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON,  //| ADC_CR2_CONT,
								ADC_CR2_EXTEN | ADC_CR2_EXTSEL | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_ADON);// | ADC_CR2_CONT);


  // Set DMA source and destination addresses.
  // Source: Address of the sine wave buffer in memory.
  //DMA2_Stream0->M0AR  = ( uint32_t )adcData;
  register_set(&(DMA2_Stream0->M0AR), ( uint32_t )rawAdcData, 0xffffffff);
  // Dest.: DAC1 Ch1 '12-bit right-aligned data' register.
  //DMA2_Stream0->PAR   = ( uint32_t )&(ADC1->DR);
  register_set(&(DMA2_Stream0->PAR), ( uint32_t )&(ADC1->DR), 0xffffffff);
  // Set DMA data transfer length
  //DMA2_Stream0->NDTR  = ( uint16_t )DMA_NUM_CH;
  register_set(&(DMA2_Stream0->NDTR), DMA_NUM_CH, 0xffff);
  // Enable DMA2 Stream 1
  //DMA2_Stream0->CR   |= ( DMA_SxCR_EN );
  //register_set(&(DMA2_Stream0->CR), DMA_SxCR_EN, 0);//DMA_SxCR_EN);
  register_set(&(DMA2_Stream0->CR),	  ( 0x0 << DMA_SxCR_CHSEL_Pos ) | 	//0 channel
		  	  	  	  	  	  	  	  ( 0x2 << DMA_SxCR_PL_Pos ) |  	//high proirity
									  ( 0x1 << DMA_SxCR_MSIZE_Pos ) |	//16 bit
									  ( 0x1 << DMA_SxCR_PSIZE_Pos ) | 	//16 bit
									  DMA_SxCR_MINC |					//memory increment
									  DMA_SxCR_CIRC |					//Circular mode
									  ( 0x00 << DMA_SxCR_DIR_Pos ) |
									  DMA_SxCR_TCIE | 		//Peripheral-to-memory
									  DMA_SxCR_EN,
								   	   	  DMA_SxCR_CHSEL |
										  DMA_SxCR_PL |
										  DMA_SxCR_MSIZE |
										  DMA_SxCR_PSIZE |
										  DMA_SxCR_MINC |
										  DMA_SxCR_PINC |
										  DMA_SxCR_CIRC |
										  DMA_SxCR_DIR |
										  DMA_SxCR_TCIE |
										  DMA_SxCR_EN);


   //use hardware timer 2 to trigger ADC 35 times per second
	//TIM3->PSC = (CORE_FREQ/2-1); //got 1Mhz
	register_set(&(TIM3->PSC), CORE_FREQ/2-1, 0xffff);
	//max timer value
	//TIM3->ARR =  100; //10-1;
	//register_set(&(TIM3->ARR), 500, 0xffff);
	register_set(&(TIM3->ARR), 500, 0xffff);

	//TIM3->DIER |= TIM_DIER_UIE;
	register_set(&(TIM3->DIER), TIM_DIER_UIE, TIM_DIER_UIE);
	//turn on TRGO signal for ADC -----------------------------------------
	//TIM3->CR2 = TIM_CR2_MMS_1; //update event
	register_set(&(TIM3->CR2), TIM_CR2_MMS_1, TIM_CR2_MMS);
	//allow timer working & reset on overflowing
	//TIM3->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
	register_set(&(TIM3->CR1), TIM_CR1_CEN | TIM_CR1_ARPE,
									TIM_CR1_CEN | TIM_CR1_ARPE);

   //first start
	ADC1->CR2 |= ADC_CR2_SWSTART;
    //register_set(&(ADC1->CR2), ADC_CR2_SWSTART, 0);// ADC_CR2_SWSTART);     //start conversions OVR

   //TIM3_IRQn
//   REGISTER_INTERRUPT(TIM3_IRQn, TIM3_IRQh, 15000U, FAULT_INTERRUPT_RATE_TICK);
//   NVIC_SetPriority(TIM3_IRQn, 14);
//   NVIC_EnableIRQ(TIM3_IRQn);

//   REGISTER_INTERRUPT(ADC_IRQn, ADC_IRQh, 15000U, FAULT_INTERRUPT_RATE_TICK);
//   NVIC_SetPriority(ADC_IRQn, 14);
//   NVIC_EnableIRQ(ADC_IRQn);

   REGISTER_INTERRUPT(DMA2_Stream0_IRQn, DMA2_Stream0_IRQh, 15000U, FAULT_INTERRUPT_RATE_TICK);
   NVIC_SetPriority(DMA2_Stream0_IRQn, 14);
   NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

//8 hz
void mishka_tick(void){
static uint32_t i;

	puth(rawAdcData[15]); puts(" "); puth(rawAdcData[14]); puts(" ");
//	puth(current_board->mishka.rawAdcData[13]); puts(" "); puth(current_board->mishka.rawAdcData[12]); puts(" ");
	puts("\n\r");
	puth(rawAdcData[0]); puts(" "); puth(DMA2_Stream0->NDTR);
	puts("\n\r");
	puth(DMA2_Stream0->PAR);
	puts("\n");
	puth(ADC1->SR);
	puts("\n");
//	puth(DMA2_Stream0->NDTR);
//    puts("\n");


	if (i%2){
		set_gpio_output(GPIOB, 14, true);
		set_gpio_output(GPIOB, 15, false);

		//set_gpio_output(GPIOC, 12, true);
	}else{
		set_gpio_output(GPIOB, 14, false);
		set_gpio_output(GPIOB, 15, true);

		//set_gpio_output(GPIOC, 12, false);
	}

i++;

}


//usb got data from EP4
void mishka_usb_get(uint8_t * data, uint8_t len){

	UNUSED(len);
	if (data[4])
		set_gpio_output(GPIOA, 9, true);
	else
		set_gpio_output(GPIOA, 9, false);
}

//int get_rtc_pkt(void *dat) {
//  timestamp_t t = rtc_get_time();
//  (void)memcpy(dat, &t, sizeof(t));
//  return sizeof(t);
//}

int mishka_usb_send(void *data){

	(void)memcpy(data, &rawAdcData, 2);
  //(uint16_t*)data[0] = rawAdcData[0];
  return 2;
}
