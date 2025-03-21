
#include "mishka_declaration.h"

int16_t momentAdd;
uint16_t mDiffLimit = MOMENT_DIFF_LIMIT;
static int32_t sum, P, I, D, res;
static uint32_t steerActionTimer = 0;

//multiply moment according wheel position
int16_t correctMoment(int16_t m, int16_t angle, uint16_t* x, uint16_t* y){
	uint8_t i;

	angle = (angle>=0)?angle:-angle;
	if (angle <= x[0])
		return m*y[0]/POINT_DATA_DIVIDER;
	else if (angle >= x[CORRECT_POINT_NUM-1])
		return m*y[CORRECT_POINT_NUM-1]/POINT_DATA_DIVIDER;
	else{ //make interpolation
		i=1;
		while ((angle > x[i]) && (i < CORRECT_POINT_NUM-1))
			i++;
		if (i > CORRECT_POINT_NUM-1)
			return m;

		return m*(y[i]+(x[i] - angle)*(y[i-1] - y[i])/(x[i]-x[i-1]))/POINT_DATA_DIVIDER;
	}
	return m;
}


int16_t abs(int16_t d){
	if (d < 0)
		return -d;
	else
		return d;
}

int16_t makePID(int16_t diff, uint8_t isReset){

static int32_t prevData, prevMoment, dArr[16], diffArr[DIFF_AVRG];
static uint8_t dIndex, diffIndex;
uint16_t pidP, i;

if (isReset == resetPid){
	sum = 0;
	prevData = 0;
	return 0;
}

//make some averaging
diffArr[(diffIndex++)%DIFF_AVRG] = diff;
diff = 0;
for (i=0;i<DIFF_AVRG;i++)
	diff += diffArr[i];
diff /= DIFF_AVRG;

if (abs(mishka.steerWheelMoment) < PID_I_DROP_ADD)
	sum += diff;

//limit sum
if (sum >= 100*MAX_MOMENT/PID_I)
	sum = 100*MAX_MOMENT/PID_I;
if (sum <= (-100*MAX_MOMENT/PID_I))
	sum = (-100*MAX_MOMENT/PID_I);

pidP = correctMoment(PID_P, mishka.steerPosition/2, pidPAngle, pidPData);
P = (pidP)*diff/100;

I = correctMoment(PID_I, mishka.steerPosition/2, pidIAngle, pidIData)*sum/100;

D = correctMoment(PID_D, mishka.steerPosition/2, pidDAngle, pidDData)*(diff-prevData)/100;

#define D_SMOOTH	2
dArr[(dIndex++) % D_SMOOTH] = D;
D=0;
for (i=0;i<D_SMOOTH;i++)
	D+=dArr[i];

res = P+I+D/D_SMOOTH;

//limit moment change
if (abs(res - prevMoment) > mDiffLimit){
	if ((res - prevMoment) > 0)
		res = prevMoment + mDiffLimit;
	else
		res = prevMoment - mDiffLimit;
}

if (res >= MAX_MOMENT)
	res = MAX_MOMENT;
if (res <= (-MAX_MOMENT))
	res = (-MAX_MOMENT);

prevData = diff;
prevMoment = res;

return res;
}

void doSteerControl(void){

static int16_t newMoment, prevSetAngle;
static uint32_t cnt;

		if ((cnt++)%2){ //to make 50 Hz
			if (mishka.currentState == activeState){
#ifdef STEER_SHAKE
				if (mishka.ldwState == LDW_LEFT_ACTIVE){
					FS_RELAY_ON;
					if (steerActionTimer % 2)
						momentAdd = MOMENT_ADD;
					else
						momentAdd = MOMENT_ADD/2;
				}else if (mishka.ldwState == LDW_RIGHT_ACTIVE){
					FS_RELAY_ON;
					if (steerActionTimer % 2)
						momentAdd = -MOMENT_ADD;
					else
						momentAdd = -MOMENT_ADD/2;
				}else if (mishka.rctaState){
					FS_RELAY_ON;
					if ((steerActionTimer) % 2){
						momentAdd = MOMENT_ADD/4;
					}else{
						momentAdd = -MOMENT_ADD/4;
					}
				}else{
					FS_RELAY_OFF;
					momentAdd = 0;
				}
#else
				FS_RELAY_OFF;
				momentAdd = 0;
#endif
			}
			steerActionTimer++;

			if (mishka.opActiveTimer > 0)
				mishka.opActiveTimer--;
			else
				mishka.opData &= ~opActive;
		}

#ifdef SBI_TEST
		if ((mishka.opData & opActive)){
#else
		if ((IS_BUT_PRESS) || (mishka.opData & opActive)){
#endif
			mishka.currentState = controlState;
			FS_RELAY_ON;
		}else{
			mishka.currentState = activeState;
		}

		if ((mishka.currentState == controlState) &&
				((mishka.speed > 700) || (IS_BUT_PRESS))){

			newMoment = makePID((mishka.steerPosition - ((mishka.steerTargetAngle+prevSetAngle)/2)), normalPid);

			if (PID_NF){
				int16_t tempMom = correctMoment(PID_NF, mishka.steerPosition/2, pidNFAngle, pidNFData)*
						(mishka.steerSensor1 - mishka.steerSensor2)/100;
				newMoment += (tempMom > LIMIT_NFB)?LIMIT_NFB:tempMom;
			}

		//to make moment at wheel more smoothly
			mishka.steerTargetMoment = ((KALMAN_S_KOEF*mishka.steerTargetMoment+
												(MAX_K_KF-KALMAN_S_KOEF)*newMoment)/MAX_K_KF);

			prevSetAngle = mishka.steerTargetAngle;

			//limit value
			if (mDiffLimit > MOMENT_DIFF_LIMIT)
				mDiffLimit = MOMENT_DIFF_LIMIT;

		}else{
			makePID(0, resetPid);//reset internal variables
			mishka.steerTargetMoment = 0;
		}
}


int16_t limitMoment(int16_t moment, int16_t value){
	if (moment >= value)
		moment = value;
	else if (moment <= -value)
		moment = -value;
	return moment;
}

/*
 * use 	7ch sensor1
 * 		7ch sensor 2
 * 		2ch steer buttons ADC
 * 	skip first measure in sequence
 */
void DMA2_Stream0_IRQh(void){
uint8_t i;
uint32_t ssSum[2] = {0,0};
uint16_t value1, value2;
static int16_t oldVal1, oldVal2;

    // Check for transfer complete interrupt
    if (DMA2->LISR & DMA_LISR_TCIF0){

    	set_gpio_output(GPIOB, 4, true);

    	for(i=0;i<6;i++){
    		ssSum[0] += mishka.rawAdcData[3+i];
    		ssSum[1] += mishka.rawAdcData[10+i];
    	}
    	mishka.steerSensor1 = KALMAN(mishka.steerSensor1, ssSum[0]/6);
    	mishka.steerSensor2 = KALMAN(mishka.steerSensor2, ssSum[1]/6);
    	mishka.steerButtonsAdc = KALMAN_SBI(mishka.steerButtonsAdc, mishka.rawAdcData[1]);

    	mishka.steerWheelMoment = mishka.steerSensor1 - mishka.steerSensor2;


	/*******************************************************************************************/
		if ((mishka.currentState == controlState)){
			momentAdd = mishka.steerTargetMoment;
		}else if (mishka.currentState == offState)
			momentAdd = 0;

		limitMoment(momentAdd, MAX_MOMENT);

		value1 = mishka.steerSensor1+momentAdd;
		value2 = mishka.steerSensor2-momentAdd;

	#define MAX_ALOW_MOMENT	4093	//must be lower than 4096
		//check overflow
		if ((mishka.steerSensor1 < MAX_ALOW_MOMENT) && (mishka.steerSensor2 < MAX_ALOW_MOMENT) &&
				(value1 < MAX_ALOW_MOMENT) && (value2 < MAX_ALOW_MOMENT)){
			DAC->DHR12R1 = value1;
			DAC->DHR12R2 = value2;

			oldVal1 = value1;
			oldVal2 = value2;
		}else{
			DAC->DHR12R1 = oldVal1;//murchik.steerSensor1;
			DAC->DHR12R2 = oldVal2;//murchik.steerSensor2;

			RED_ON;
		}

		set_gpio_output(GPIOB, 4, false);

        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;  // Clear transfer complete flag
    }
}

void TIM3_IRQh(void){

static uint32_t i;

	if (TIM3->SR & TIM_SR_UIF){
		if (ADC1->SR & ADC_SR_OVR){
			ADC1->SR &= ~(ADC_SR_OVR);
			puts("ADC_OVR");
		}

		if ((mishka.flags & runMomentCalcFlag) || ((i%125) == 0)){
			set_gpio_output(GPIOC, 12, true);
			doSteerControl();
			mishka.flags &= ~runMomentCalcFlag;
			i = 0;
		}
		set_gpio_output(GPIOC, 12, false);

		TIM3->SR &= ~TIM_SR_UIF;
		i++;
	}
}

// ***************************** main code *****************************

void mishka_init(void){

	 set_gpio_mode(GPIOA, TENZO1_ADC_CH, MODE_ANALOG);
	 set_gpio_mode(GPIOA, TENZO2_ADC_CH, MODE_ANALOG);
	 set_gpio_mode(GPIOA, SBI_ADC_CH, MODE_ANALOG);

	 //init DAC
	 register_set(&(DAC->CR), DAC_CR_EN1 | DAC_CR_EN2, 0x3FFF3FFFU);

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


uint8_t getAccKey(void){
	if (mishka.steerButtonsAdc > (BTN_NOKEY_LVL+BTN_UP_LVL)/2)
		return noKey;
	else if ((mishka.steerButtonsAdc <= (BTN_NOKEY_LVL+BTN_UP_LVL)/2) &&
			(mishka.steerButtonsAdc > (BTN_UP_LVL+BTN_DOWN_LVL)/2))
		return upKey;
	else if ((mishka.steerButtonsAdc <= (BTN_UP_LVL+BTN_DOWN_LVL)/2) &&
			(mishka.steerButtonsAdc > (BTN_DOWN_LVL+BTN_CANCEL_LVL)/2))
		return downKey;
	else if ((mishka.steerButtonsAdc <= (BTN_DOWN_LVL+BTN_CANCEL_LVL)/2) &&
			(mishka.steerButtonsAdc > (BTN_CANCEL_LVL+BTN_ACC_LVL)/2))
		return cancelKey;
	else if (mishka.steerButtonsAdc > BTN_ACC_LVL/2)
		return accOnKey;
	else
		return lkasOnKey;
}

static uint32_t statusCnt;
static uint8_t onState;
static uint8_t oldSteerKey;
	   uint8_t steerKey;
static uint16_t bntPressCnt;

//8 hz
void mishka_tick(void){
static uint32_t i;

	steerKey = getAccKey();
	if (((steerKey == lkasOnKey)) && (oldSteerKey == noKey)){
		onState ^= 1;
		statusCnt = 0;
	}

//	if ((statusCnt > 5) && (onState) && ((mishka.currentState != controlState)) && (!(IS_BUT_PRESS))){ //500 ms
//		onState = 0;
//	}

	if (mishka.currentState == controlState){
	//if (onState){
		GREEN_ON;
	}else{
		GREEN_OFF;
	}

	if (steerKey){
		bntPressCnt++;
	}else{
		bntPressCnt = 0;
	}

	oldSteerKey = steerKey;


	if (i%2){
		puts("\n\r");
		//puth(mishka.steerPosition); puts(" ");puth(mishka.steerTargetAngle);
		puts("steerTargetMoment=");puth(mishka.steerTargetMoment);puts("\n\r");
		puts("steerTargetAngle=");puth(mishka.steerTargetAngle);puts("\n\r");
		puts("steerPosition=");puth(mishka.steerPosition);puts("\n\r");
		puts("speed=");puth(mishka.speed);puts("\n\r");
		puts("currentState=");puth(mishka.currentState);puts("\n\r");
		puts("\n\r");
	//	puth(steerKey); puts(" "); puth(mishka.opData);
	//	puts("\n\r");
	//	puts("\n\r");
	//	puth(mishka.rawAdcData[0]); puts(" "); puth(mishka.rawAdcData[1]);puts(" ");
	//	puth(mishka.rawAdcData[2]); puts(" "); puth(mishka.rawAdcData[3]);puts(" "); puth(mishka.rawAdcData[4]); puts(" "); puth(mishka.rawAdcData[5]);puts(" ");
	//	puth(mishka.rawAdcData[6]); puts(" "); puth(mishka.rawAdcData[7]);puts(" "); puth(mishka.rawAdcData[8]);
	//	puts("\n\r");
	//	puth(mishka.rawAdcData[9]); puts(" "); puth(mishka.rawAdcData[10]);puts(" "); puth(mishka.rawAdcData[11]); puts(" "); puth(mishka.rawAdcData[12]);puts(" ");
	//	puth(mishka.rawAdcData[13]); puts(" "); puth(mishka.rawAdcData[14]);puts(" "); puth(mishka.rawAdcData[15]);
	//	puts("\n\r");
	//	puts("\n\r");
	}

//	puth(DMA2_Stream0->PAR);
//	puts("\n");
//	puth(ADC1->SR);
//	puts("\n");
//	puth(DMA2_Stream0->NDTR);
//    puts("\n");

//	if (i%2){
//		set_gpio_output(GPIOB, 14, true);
//		set_gpio_output(GPIOB, 15, false);
//
//		//set_gpio_output(GPIOC, 12, true);
//	}else{
//		set_gpio_output(GPIOB, 14, false);
//		set_gpio_output(GPIOB, 15, true);
//
//		//set_gpio_output(GPIOC, 12, false);
//	}

i++;
}

//usb got data from EP4 use MishkaSendData struct
void mishka_usb_get(uint8_t * data, uint8_t len){

	UNUSED(len);
	UNUSED(data);
//	if (data[4])
//		set_gpio_output(GPIOA, 9, true);
//	else
//		set_gpio_output(GPIOA, 9, false);
}


//send data to comma use MishkaGetData struct
int mishka_usb_send(void *data){

	MishkaData md = {.pressedButton = steerKey, .activateOP = onState, .crc = 0x1983};
	(void)memcpy(data, &md, sizeof(MishkaData));
  //(uint16_t*)data[0] = rawAdcData[0];
  return 2;
}
