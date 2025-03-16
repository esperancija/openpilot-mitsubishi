


#define DMA_NUM_CH 16


#define MAX_K_KF		32768

#define KALMAN_SBI_KOEF 	32200
#define KALMAN_SBI(z, x) ((KALMAN_SBI_KOEF*z+(MAX_K_KF-KALMAN_SBI_KOEF)*x)/MAX_K_KF)

#define KALMAN_KOEF 1000
#define KALMAN(z, x) ((KALMAN_KOEF*z+(MAX_K_KF-KALMAN_KOEF)*x)/MAX_K_KF)

typedef struct{
///state of device
	uint16_t rawAdcData[DMA_NUM_CH];
	float steerSensor1;
	float steerSensor2;
	float steerButtonsAdc; //steer button input
	uint16_t sbo; //steer button output

	uint16_t speed;					//from CAN
	int16_t steerPosition;          //from CAN
	uint16_t steerSpeed;            //from CAN
	int16_t steerMoment;            //from CAN

	int16_t steerTargetAngle;       //calculate needed angle
	uint16_t steerTargetTime;       //in read ldw data period 1/10s
	int16_t steerTargetMoment; 		//
	int16_t steerWheelMoment;       //

	uint8_t  currentState;
	uint8_t  steerButton;
	uint8_t  opData;
	uint8_t  key;
	uint8_t  oldKey;
	uint8_t  flags;
}Mishka;

Mishka mishka;

/*
 * use 	7ch sensor1
 * 		7ch sensor 2
 * 		2ch steer buttons ADC
 * 	skip first measure in sequence
 */
void DMA2_Stream0_IRQh(void){
uint8_t i;
uint32_t ssSum[2] = {0,0};
    // Check for transfer complete interrupt
    if (DMA2->LISR & DMA_LISR_TCIF0){
    	set_gpio_output(GPIOC, 12, false);

    	for(i=0;i<6;i++){
    		ssSum[0] += mishka.rawAdcData[3+i];
    		ssSum[1] += mishka.rawAdcData[10+i];
    	}
    	mishka.steerSensor1 = KALMAN(mishka.steerSensor1, ssSum[0]/6);
    	mishka.steerSensor2 = KALMAN(mishka.steerSensor2, ssSum[1]/6);
    	mishka.steerButtonsAdc = KALMAN_SBI(mishka.steerButtonsAdc, mishka.rawAdcData[1]);

        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;  // Clear transfer complete flag
    }
}

void TIM3_IRQh(void){

	if (TIM3->SR & TIM_SR_UIF){
		if (ADC1->SR & ADC_SR_OVR){
			ADC1->SR &= ~(ADC_SR_OVR);
			puts("ADC_OVR");
		}
		set_gpio_output(GPIOC, 12, true);

		TIM3->SR &= ~TIM_SR_UIF;
	}
}

// ***************************** main code *****************************

#define TENZO1_ADC_CH	1
#define TENZO2_ADC_CH	6
#define SBI_ADC_CH		7

void mishka_init(void){

	 set_gpio_mode(GPIOA, TENZO1_ADC_CH, MODE_ANALOG);
	 set_gpio_mode(GPIOA, TENZO2_ADC_CH, MODE_ANALOG);
	 set_gpio_mode(GPIOA, SBI_ADC_CH, MODE_ANALOG);

    register_set(&(ADC1->CR1), ADC_CR1_SCAN | ADC_CR1_EOCIE,
    									ADC_CR1_SCAN | ADC_CR1_EOCIE);

    register_set(&(ADC1->SQR1),
    		((DMA_NUM_CH-1) << ADC_SQR1_L_Pos) |
    		(TENZO1_ADC_CH << ADC_SQR1_SQ16_Pos)  |
			(TENZO1_ADC_CH << ADC_SQR1_SQ15_Pos)  |
			(TENZO1_ADC_CH << ADC_SQR1_SQ14_Pos)  |
			(TENZO1_ADC_CH << ADC_SQR1_SQ13_Pos)
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


#define ADC_CYCLES	7
    register_set(&(ADC1->SMPR1),
        (ADC_CYCLES << (3 * TENZO1_ADC_CH)) |
        (ADC_CYCLES << (3 * TENZO2_ADC_CH)) |
        (ADC_CYCLES << (3 * SBI_ADC_CH)),
        (0x7 << (3 * TENZO1_ADC_CH)) |
        (0x7 << (3 * TENZO2_ADC_CH)) |
        (0x7 << (3 * SBI_ADC_CH))
    );

  // Set DMA source and destination addresses.

  register_set(&(DMA2_Stream0->M0AR), ( uint32_t )mishka.rawAdcData, 0xffffffff);
  register_set(&(DMA2_Stream0->PAR), ( uint32_t )&(ADC1->DR), 0xffffffff);

  // Set DMA data transfer length
  register_set(&(DMA2_Stream0->NDTR), DMA_NUM_CH, 0xffff);

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


   //use hardware timer 2 to trigger ADC
	register_set(&(TIM3->PSC), CORE_FREQ/2-1, 0xffff); // 1MHz
	//set period in uS
	register_set(&(TIM3->ARR), 100, 0xffff);

	register_set(&(TIM3->DIER), TIM_DIER_UIE, TIM_DIER_UIE);
	//turn on TRGO signal for ADC -----------------------------------------
	register_set(&(TIM3->CR2), TIM_CR2_MMS_1, TIM_CR2_MMS);
	//allow timer working & reset on overflowing
	register_set(&(TIM3->CR1), TIM_CR1_CEN | TIM_CR1_ARPE,
									TIM_CR1_CEN | TIM_CR1_ARPE);

   //first start
	ADC1->CR2 |= ADC_CR2_SWSTART;

   //TIM3_IRQn
   REGISTER_INTERRUPT(TIM3_IRQn, TIM3_IRQh, 15000U, FAULT_INTERRUPT_RATE_TICK);
   NVIC_SetPriority(TIM3_IRQn, 14);
   NVIC_EnableIRQ(TIM3_IRQn);

   REGISTER_INTERRUPT(DMA2_Stream0_IRQn, DMA2_Stream0_IRQh, 15000U, FAULT_INTERRUPT_RATE_TICK);
   NVIC_SetPriority(DMA2_Stream0_IRQn, 14);
   NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

//8 hz
void mishka_tick(void){
static uint32_t i;


//	puts("\n\r");
//	puth(mishka.steerButtonsAdc); puts(" ");
//	puts("\n\r");
//	puth(mishka.rawAdcData[0]); puts(" "); puth(mishka.rawAdcData[1]);puts(" ");
//	puth(mishka.rawAdcData[2]); puts(" "); puth(mishka.rawAdcData[3]);puts(" "); puth(mishka.rawAdcData[4]); puts(" "); puth(mishka.rawAdcData[5]);puts(" ");
//	puth(mishka.rawAdcData[6]); puts(" "); puth(mishka.rawAdcData[7]);puts(" "); puth(mishka.rawAdcData[8]);
//	puts("\n\r");
//	puth(mishka.rawAdcData[9]); puts(" "); puth(mishka.rawAdcData[10]);puts(" "); puth(mishka.rawAdcData[11]); puts(" "); puth(mishka.rawAdcData[12]);puts(" ");
//	puth(mishka.rawAdcData[13]); puts(" "); puth(mishka.rawAdcData[14]);puts(" "); puth(mishka.rawAdcData[15]);
//	puts("\n\r");
//	puts("\n\r");

//	puth(DMA2_Stream0->PAR);
//	puts("\n");
//	puth(ADC1->SR);
//	puts("\n");
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


//usb got data from EP4 use MishkaSendData struct
void mishka_usb_get(uint8_t * data, uint8_t len){

	UNUSED(len);
	if (data[4])
		set_gpio_output(GPIOA, 9, true);
	else
		set_gpio_output(GPIOA, 9, false);
}


//send data to comma use MishkaGetData struct
int mishka_usb_send(void *data){

uint16_t tmp = 	mishka.steerButtonsAdc;

	(void)memcpy(data, &tmp, 2);
  //(uint16_t*)data[0] = rawAdcData[0];
  return 2;
}
