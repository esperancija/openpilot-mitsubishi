
//
//#ifndef __MISHKA__
//#define __MISHKA__

#define DMA_NUM_CH 16

typedef struct{
///state of device
	uint16_t rawAdcData[DMA_NUM_CH];
	float steerSensor1;
	float steerSensor2;
	float steerButtonsAdc; //steer button input
	uint16_t sbo; //steer button output

	uint16_t 	speed;					//from CAN
	int16_t 	steerPosition;          //from CAN
	int16_t 	steerMoment;            //from CAN

	int16_t 	steerTargetAngle;      //calculate needed angle
	int16_t 	steerTestAngle;       //calculate needed angle
	uint16_t 	steerTargetTime;       //in read ldw data period 1/10s
	int16_t 	steerTargetMoment; 		//
	int16_t 	steerWheelMoment;       //
	uint16_t 	opActiveTimer;

	uint8_t  currentState;
	uint8_t  steerButton;
	uint8_t  opData;
	uint8_t  key;
	uint8_t  oldKey;
	uint8_t  flags;
}Mishka;


typedef struct{
  uint8_t pressedButton;
  bool activateOP;
  uint32_t crc;
}MishkaData;

enum Flags {runMomentCalcFlag = 1, callTimeOutFlag = 2};
enum PidReset {normalPid, resetPid};

enum State {
	offState, 		//no control
	activeState, 	//steer shake by ldw warning
	controlState, 	//steer control by external data
	testState,
				lastState
};

enum Key {noKey = 0, lkasOnKey, cancelKey, accOnKey, upKey, downKey};

enum OPState {
	opActive = 1, opLeftLine = 2, opRightLine = 4
};

Mishka mishka;

#define BTN_ACC_LVL		736//675//750
#define BTN_CANCEL_LVL	1364//1200//1400
#define BTN_DOWN_LVL	2025 //1940//1600//2010
#define BTN_UP_LVL		2482//2000//2570
#define BTN_NOKEY_LVL	2980//2300

#define CORRECT_POINT_NUM	5
#define POINT_DATA_DIVIDER	10

#define MAX_K_KF		32768

#define KALMAN_SBI_KOEF 	30000
#define KALMAN_SBI(z, x) ((KALMAN_SBI_KOEF*z+(MAX_K_KF-KALMAN_SBI_KOEF)*x)/MAX_K_KF)

#define KALMAN_KOEF 1000
#define KALMAN(z, x) ((KALMAN_KOEF*z+(MAX_K_KF-KALMAN_KOEF)*x)/MAX_K_KF)

#define KALMAN_S_KOEF 	21000
#define KALMAN_S(z, x) ((KALMAN_S_KOEF*z+(MAX_K_KF-KALMAN_S_KOEF)*x)/MAX_K_KF)

#define TARGET_LEVEL		3115 //in adc to get 2.5V MUST be > 2048
#define MAX_MOMENT			(10*(4095-TARGET_LEVEL)/10)//(9*(4095-TARGET_LEVEL)/10)//(8*(4095-TARGET_LEVEL)/10) MODER12

#define PID_P 	600
uint16_t pidPAngle[] = {0,   7, 15, 30, 70}; //in degree
uint16_t pidPData[] =  {40, 60, 45,  37, 30};

#define PID_I	125
uint16_t pidIAngle[] = {0,   3, 7, 30, 70}; //in degree
uint16_t pidIData[] =  {10, 10, 10,  5, 5};  //in 1/10

#define PID_D	13000
uint16_t pidDAngle[] = {0,   7, 15, 30, 70}; //in degree
uint16_t pidDData[] =  {15, 15, 13,  12, 10};  //in 1/10

#define PID_NF			10
uint16_t pidNFAngle[] = {0, 5, 10, 20, 90}; //in degree
uint16_t pidNFData[] =  {70, 10,  0,  0, 0};  //in 1/10

#define PID_I_DROP_ADD	2000///to prevent increase I component moment at driver action
#define LIMIT_NFB		800

#define MOMENT_DIFF_LIMIT	1000//155//105//150 //150 //limit moment change per step

//moment to make shake action at working LDW & RCTA
#define MOMENT_ADD				(350 + mishka.speed/50)

#define DIFF_AVRG	3
#define TENZO1_ADC_CH	6 //1
#define TENZO2_ADC_CH	1 //6
#define SBI_ADC_CH		7

#define STEERING_WHEEL_MOMENT_ID	0x2f1 //C+D
#define STEERING_WHEEL_POS_ID		0x236	//A+B
#define SPEED_ID					0x214
#define STEER_CONTROL_ID			0x3b6


#define IS_BUT_PRESS	!get_gpio_input(GPIOA, 10)
#define FS_RELAY_ON		set_gpio_output(GPIOA, 9, false)
#define FS_RELAY_OFF	set_gpio_mode(GPIOA, 9, MODE_INPUT); //set_gpio_output(GPIOA, 9, true)
#define FS_RELAY_STATE	get_gpio_input(GPIOA, 9)//	(GPIOA->IDR & GPIO_IDR_IDR_9)

/*
//#define FS_RELAY_SETUP 	do {GPIOA->MODER &= ~GPIO_MODER_MODER9;\
//									GPIOA->MODER |= GPIO_MODER_MODER9_0;} while (0)
//#define FS_RELAY_ON		do {GPIOA->MODER &= ~GPIO_MODER_MODER9;\
//								GPIOA->MODER |= GPIO_MODER_MODER9_0;\
//								GPIOA->BSRR = GPIO_BSRR_BS_9;} while (0)
//#define FS_RELAY_OFF		(GPIOA->MODER &= ~GPIO_MODER_MODER9)
 * */


#define GREEN_ON 			set_gpio_output(GPIOB, 14, true)
#define GREEN_OFF 			set_gpio_output(GPIOB, 14, false)
#define RED_ON 				set_gpio_output(GPIOB, 15, true)
#define RED_OFF 			set_gpio_output(GPIOB, 15, false);

#define OP_ACTIVE_TIMEOUT	20 //in ? sec

//init code
void mishka_init(void);

//need call at CAN packet with speed receiving
void doSteerControl(void);

//call when got data from USB EP4
void mishka_usb_get(uint8_t * data, uint8_t len);

//call when got mishka data request at USB control point
int mishka_usb_send(void *data);

void mishka_tick(void);

//#endif
