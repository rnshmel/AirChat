// AIR CHAT PCB FIRMWARE
// written by Richard Shmel

#include "main.h"
#include <string.h>


SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim16;
UART_HandleTypeDef huart1;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM16_Init(void);

/*----------------------[CC1100 - config register]----------------------------*/
#define IOCFG2   0x00         // GDO2 output pin configuration
#define IOCFG1   0x01         // GDO1 output pin configuration
#define IOCFG0   0x02         // GDO0 output pin configuration
#define FIFOTHR  0x03         // RX FIFO and TX FIFO thresholds
#define SYNC1    0x04         // Sync word, high byte
#define SYNC0    0x05         // Sync word, low byte
#define PKTLEN   0x06         // Packet length
#define PKTCTRL1 0x07         // Packet automation control
#define PKTCTRL0 0x08         // Packet automation control
#define ADDR     0x09         // Device address
#define CHANNR   0x0A         // Channel number
#define FSCTRL1  0x0B         // Frequency synthesizer control
#define FSCTRL0  0x0C         // Frequency synthesizer control
#define FREQ2    0x0D         // Frequency control word, high byte
#define FREQ1    0x0E         // Frequency control word, middle byte
#define FREQ0    0x0F         // Frequency control word, low byte
#define MDMCFG4  0x10         // Modem configuration
#define MDMCFG3  0x11         // Modem configuration
#define MDMCFG2  0x12         // Modem configuration
#define MDMCFG1  0x13         // Modem configuration
#define MDMCFG0  0x14         // Modem configuration
#define DEVIATN  0x15         // Modem deviation setting
#define MCSM2    0x16         // Main Radio Cntrl State Machine config
#define MCSM1    0x17         // Main Radio Cntrl State Machine config
#define MCSM0    0x18         // Main Radio Cntrl State Machine config
#define FOCCFG   0x19         // Frequency Offset Compensation config
#define BSCFG    0x1A         // Bit Synchronization configuration
#define AGCCTRL2 0x1B         // AGC control
#define AGCCTRL1 0x1C         // AGC control
#define AGCCTRL0 0x1D         // AGC control
#define WOREVT1  0x1E         // High byte Event 0 timeout
#define WOREVT0  0x1F         // Low byte Event 0 timeout
#define WORCTRL  0x20         // Wake On Radio control
#define FREND1   0x21         // Front end RX configuration
#define FREND0   0x22         // Front end TX configuration
#define FSCAL3   0x23         // Frequency synthesizer calibration
#define FSCAL2   0x24         // Frequency synthesizer calibration
#define FSCAL1   0x25         // Frequency synthesizer calibration
#define FSCAL0   0x26         // Frequency synthesizer calibration
#define RCCTRL1  0x27         // RC oscillator configuration
#define RCCTRL0  0x28         // RC oscillator configuration
#define FSTEST   0x29         // Frequency synthesizer cal control
#define PTEST    0x2A         // Production test
#define AGCTEST  0x2B         // AGC test
#define TEST2    0x2C         // Various test settings
#define TEST1    0x2D         // Various test settings
#define TEST0    0x2E         // Various test settings
/*-------------------------[END config register]------------------------------*/


/*------------------------[CC1100-command strobes]----------------------------*/
#define SRES     0x30         // Reset chip
#define SFSTXON  0x31         // Enable/calibrate freq synthesizer
#define SXOFF    0x32         // Turn off crystal oscillator.
#define SCAL     0x33         // Calibrate freq synthesizer & disable
#define SRX      0x34         // Enable RX.
#define STX      0x35         // Enable TX.
#define SIDLE    0x36         // Exit RX / TX
#define SAFC     0x37         // AFC adjustment of freq synthesizer
#define SWOR     0x38         // Start automatic RX polling sequence
#define SPWD     0x39         // Enter pwr down mode when CSn goes hi
#define SFRX     0x3A         // Flush the RX FIFO buffer.
#define SFTX     0x3B         // Flush the TX FIFO buffer.
#define SWORRST  0x3C         // Reset real time clock.
#define SNOP     0x3D         // No operation.
/*-------------------------[END command strobes]------------------------------*/

/*----------------------[CC1100 - status register]----------------------------*/
#define PARTNUM        0xF0   // Part number
#define VERSION        0xF1   // Current version number
#define FREQEST        0xF2   // Frequency offset estimate
#define LQI            0xF3   // Demodulator estimate for link quality
#define RSSI           0xF4   // Received signal strength indication
#define MARCSTATE      0xF5   // Control state machine state
#define WORTIME1       0xF6   // High byte of WOR timer
#define WORTIME0       0xF7   // Low byte of WOR timer
#define PKTSTATUS      0xF8   // Current GDOx status and packet status
#define VCO_VC_DAC     0xF9   // Current setting from PLL cal module
#define TXBYTES        0xFA   // Underflow and # of bytes in TXFIFO
#define RXBYTES        0xFB   // Overflow and # of bytes in RXFIFO
#define RCCTRL1_STATUS 0xFC   //Last RC Oscillator Calibration Result
#define RCCTRL0_STATUS 0xFD   //Last RC Oscillator Calibration Result
//--------------------------[END status register]-------------------------------

/*----------------------[CC1100 - SMART RF settings]----------------------------*/
#define SMARTRF_SETTING_IOCFG2           0x06 // asserts when sync word received, used to switch into "get bytes from RF FIFO" state
#define SMARTRF_SETTING_IOCFG1           0x2E
#define SMARTRF_SETTING_IOCFG0_CCA       0x09 // GDO0 = CCA when 0x09	GDO0 = TX FIFO THRESH when 0x02		PLL lock 0x0A
#define SMARTRF_SETTING_IOCFG0_TXF		 0x02
#define SMARTRF_SETTING_FIFOTHR          0x47
#define SMARTRF_SETTING_SYNC1            0xD3
#define SMARTRF_SETTING_SYNC0            0x91
#define SMARTRF_SETTING_PKTLEN           0xFF
#define SMARTRF_SETTING_PKTCTRL1         0x40
#define SMARTRF_SETTING_PKTCTRL0         0x01
#define SMARTRF_SETTING_ADDR             0x00
#define SMARTRF_SETTING_CHANNR           0x00
#define SMARTRF_SETTING_FSCTRL1          0x0F
#define SMARTRF_SETTING_FSCTRL0          0x70 // offset 70 = 913MHz for CH0
#define SMARTRF_SETTING_FREQ2            0x23
#define SMARTRF_SETTING_FREQ1            0x1D
#define SMARTRF_SETTING_FREQ0            0x88
#define SMARTRF_SETTING_MDMCFG4          0xC7 // C3 = 300 (test only), C5 = 1200, C8 = 9600, C9 = 19200, CA = 38400, C9 = 19200, C6 = 2400
#define SMARTRF_SETTING_MDMCFG3          0x83
#define SMARTRF_SETTING_MDMCFG2          0x03 // sync word settings
#define SMARTRF_SETTING_MDMCFG1          0x42 // preamable and channel spacing
#define SMARTRF_SETTING_MDMCFG0          0xF8
#define SMARTRF_SETTING_DEVIATN          0x40
#define SMARTRF_SETTING_MCSM2            0x07
#define SMARTRF_SETTING_MCSM1            0x20 // CCA mode and RX/TX OFF
#define SMARTRF_SETTING_MCSM0            0x08 // auto-cal settings
#define SMARTRF_SETTING_FOCCFG           0x36
#define SMARTRF_SETTING_BSCFG            0x6C
#define SMARTRF_SETTING_AGCCTRL2         0x03
#define SMARTRF_SETTING_AGCCTRL1         0x68 // CS thresholds
#define SMARTRF_SETTING_AGCCTRL0         0x91
#define SMARTRF_SETTING_WOREVT1          0x87
#define SMARTRF_SETTING_WOREVT0          0x6B
#define SMARTRF_SETTING_WORCTRL          0xFB
#define SMARTRF_SETTING_FREND1           0x56
#define SMARTRF_SETTING_FREND0           0x10
#define SMARTRF_SETTING_FSCAL3           0xE9
#define SMARTRF_SETTING_FSCAL2           0x2A
#define SMARTRF_SETTING_FSCAL1           0x00
#define SMARTRF_SETTING_FSCAL0           0x1F
#define SMARTRF_SETTING_RCCTRL1          0x41
#define SMARTRF_SETTING_RCCTRL0          0x00
#define SMARTRF_SETTING_FSTEST           0x59
#define SMARTRF_SETTING_PTEST            0x7F
#define SMARTRF_SETTING_AGCTEST          0x3F
#define SMARTRF_SETTING_TEST2            0x81
#define SMARTRF_SETTING_TEST1            0x35
#define SMARTRF_SETTING_TEST0            0x09
//--------------------------[END SMART RF]-------------------------------

/*----------------------[CC1100 - misc]----------------------------*/
#define BURST_TX_FIFO 				0x7F // used to burst access TX FIFO
#define SINGLE_RX_FIFO 				0xBF // Single byte access to RX FIFO
#define WRITE_TXFIFO_CHUNK_SIZE 	24 // used to burst access TX FIFO
#define MAX_MSG_LENGTH 				255 // max message size (far shorter in reality, limited by user input len)
#define MDMCFG4_2FSK 				0xF8
#define MDMCFG2_2FSK 				0x03
#define AGCCTRL2_2FSK 				0x03
#define AGCCTRL1_2FSK 				0x68
#define AGCCTRL0_2FSK 				0x91
#define FREND0_2FSK 				0x10
#define MDMCFG4_OOK 				0xF7
#define MDMCFG2_OOK 				0x33
#define AGCCTRL2_OOK 				0x03 // 0x03 to 0x07
#define AGCCTRL1_OOK 				0x00 // 0x00
#define AGCCTRL0_OOK 				0x91 // 0x91 or 0x92
#define FREND0_OOK 					0x11

//#define MAX_RX_TIMEOUT 				10 // fixed timeout used for RX packet end
//#define RX_POLL_TIME 				2500 // fixed time for RX polling (in uS) -->  ((1/baud)*8)/3
/*-------------------------[END misc]------------------------------*/
void cc1101_init(SPI_HandleTypeDef hspi, uint8_t PATables[], uint8_t channr, uint8_t mdmcfg4, uint8_t mdmcfg2, uint8_t agcctrl2, uint8_t agcctrl1, uint8_t agcctrl0, uint8_t frend0)
{
	HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
	// temp value
	uint8_t val = 0;

	// configuration table
	uint8_t configTable[] = {SMARTRF_SETTING_IOCFG2,SMARTRF_SETTING_IOCFG1,SMARTRF_SETTING_IOCFG0_CCA,SMARTRF_SETTING_FIFOTHR,SMARTRF_SETTING_SYNC1,SMARTRF_SETTING_SYNC0,
			SMARTRF_SETTING_PKTLEN,SMARTRF_SETTING_PKTCTRL1,SMARTRF_SETTING_PKTCTRL0,SMARTRF_SETTING_ADDR,channr,SMARTRF_SETTING_FSCTRL1,SMARTRF_SETTING_FSCTRL0,
			SMARTRF_SETTING_FREQ2,SMARTRF_SETTING_FREQ1,SMARTRF_SETTING_FREQ0,mdmcfg4,SMARTRF_SETTING_MDMCFG3,mdmcfg2,SMARTRF_SETTING_MDMCFG1,
			SMARTRF_SETTING_MDMCFG0,SMARTRF_SETTING_DEVIATN,SMARTRF_SETTING_MCSM2,SMARTRF_SETTING_MCSM1,SMARTRF_SETTING_MCSM0,SMARTRF_SETTING_FOCCFG,SMARTRF_SETTING_BSCFG,
			agcctrl2,agcctrl1,agcctrl0,SMARTRF_SETTING_WOREVT1,SMARTRF_SETTING_WOREVT0,SMARTRF_SETTING_WORCTRL,SMARTRF_SETTING_FREND1,
			frend0,SMARTRF_SETTING_FSCAL3,SMARTRF_SETTING_FSCAL2,SMARTRF_SETTING_FSCAL1,SMARTRF_SETTING_FSCAL0,SMARTRF_SETTING_RCCTRL1,SMARTRF_SETTING_RCCTRL0};

	// init the CC1101
	HAL_GPIO_WritePin(TLED_GPIO_Port, TLED_Pin, GPIO_PIN_SET); // turn on TX LED
	HAL_GPIO_WritePin(RLED_GPIO_Port, RLED_Pin, GPIO_PIN_SET); // turn on RX LED
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET); // make sure SPI is logic 1
	HAL_Delay(10);

	// startup sequence
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_Delay(10);

	// soft reset of the chip
	val = SRES;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &val, 1, 1000); // TX
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	// load all config registers with HAL SPI loop
	for(uint8_t reg = 0; reg < sizeof(configTable)/sizeof(configTable[0]); reg++)
	{
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi, &reg,1,1000);
		val = configTable[reg];
		HAL_SPI_Transmit(&hspi, &val,1,1000);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		//HAL_Delay(2);
	}
	HAL_SPIEx_FlushRxFifo(&hspi);

	// set PA table
	// first value is 0x7E, which is the PA reg addr (0x3E) with the bitmask for burst (0x40) applied
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, PATables, 9, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	// set RF chip idle
	val = SIDLE;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &val, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	// run calibrate on chipset
	val = SCAL;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &val, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(100);

	// set RF chip idle
	val = SIDLE;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &val, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	// flush RF RX buffer
	val = SFRX;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &val, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	// set RF chip to RX
	val = SRX;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &val, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

	// init clean up tasks
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_Delay(10);
	HAL_SPIEx_FlushRxFifo(&hspi);
	HAL_GPIO_WritePin(TLED_GPIO_Port, TLED_Pin, GPIO_PIN_RESET); // turn off TX LED
	HAL_GPIO_WritePin(RLED_GPIO_Port, RLED_Pin, GPIO_PIN_RESET); // turn off RX LED
	__HAL_GPIO_EXTI_CLEAR_RISING_IT(GDO2_Pin);
	HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

void cc1101_reset_RX(SPI_HandleTypeDef hspi)
{
	// loop: check if idle
	// strobe 0xF5 - MARC state
	uint8_t StrobeVal = 0xF5;
	uint8_t CC1101StatusBytes[2] = {0};
	while(1)
	{
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi, &StrobeVal, 1, 1000); // TX
		HAL_SPI_Receive(&hspi, CC1101StatusBytes, 2, 1000); // RX
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		if (CC1101StatusBytes[0] == 1)
		{
			break;
		}
	}
	// flush RF RX buffer
	StrobeVal = SFRX;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &StrobeVal, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

	// set RF chip to RX
	StrobeVal = SRX;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &StrobeVal, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

	// flush SPI buffer
	HAL_SPIEx_FlushRxFifo(&hspi);
}

void cc1101_TX_packet(SPI_HandleTypeDef hspi, uint8_t * txFifoBuffer)
{
	HAL_GPIO_WritePin(TLED_GPIO_Port, TLED_Pin, GPIO_PIN_SET); // turn on TX LED
	uint8_t strobeVal = 0;
	uint8_t cc1101StatusByte = 0;
	uint8_t cc1101StatusBytes[2] = {0};
	uint8_t bytesRemain = txFifoBuffer[1]; // used in data packets > 63 bytes
	uint8_t bytesPos = 0; // used in data packets > 63 bytes
	uint8_t writeChunkSize = WRITE_TXFIFO_CHUNK_SIZE;
	// byte 0 = BURST_TX_FIFO
	// byte 1 = data length
	// byte[2:] = data

	// strobe idle
	strobeVal = SIDLE;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
	HAL_SPI_Receive(&hspi, &cc1101StatusByte, 1, 1000); // RX
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

	// data <= 63 bytes
	// single burst
	if (txFifoBuffer[1] <= 63)
	{
		// load into TX fifo with a burst command
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi, txFifoBuffer, txFifoBuffer[1]+2, 1000); // TX
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_SPIEx_FlushRxFifo(&hspi);

		// strobe TX and send the data burst
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		strobeVal = STX;
		HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
		HAL_SPI_Receive(&hspi, &cc1101StatusByte, 1, 1000); // RX
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

		// poll TXBYTES to see if the burst was sent
		// strobe TXBYTES
		strobeVal = TXBYTES;
		while (1)
		{
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
		  HAL_SPI_Receive(&hspi, cc1101StatusBytes, 2, 1000); // RX
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  if (cc1101StatusBytes[0] == 0)
		  {
			  // TXBYTES is zero
			  HAL_SPIEx_FlushRxFifo(&hspi);
			  break;
		  }
		}
		// poll SNOP to get radio status
		// radio drops to IDLE when TX is done
		strobeVal = SNOP;
		while (1)
		{
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
		  HAL_SPI_Receive(&hspi, &cc1101StatusByte, 1, 1000); // RX
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  if ((cc1101StatusByte & 0xF0) == 0)
		  {
			  break;
		  }
		}

	} //end if single burst
	else
	{
		// data is > 64
		// have to fill TX FIFO during send
		// load only first 64 bytes into TX fifo with a burst command
		// includes: header (1), size byte (1), data (63)
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi, txFifoBuffer, 65, 1000); // TX
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		bytesRemain -= 63; // subtract from bytes remain
		bytesPos += 65; // add bytes position
		HAL_SPIEx_FlushRxFifo(&hspi);

		// strobe TX and send the data burst
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		strobeVal = STX;
		HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
		HAL_SPI_Receive(&hspi, &cc1101StatusByte, 1, 1000); // RX
		HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

		// periodically strobe TXBYTES
		// if TXBYTES falls under threshold (32), add WRITE_TXFIFO_CHUNK_SIZE bytes to FIFO
		// if there are less than WRITE_TXFIFO_CHUNK_SIZE bytes to load, load remaining and break to final TXBYTES poll
		while (1)
		{
			strobeVal = TXBYTES;
			HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
			HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
			HAL_SPI_Receive(&hspi, cc1101StatusBytes, 2, 1000); // RX
			HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
			if (cc1101StatusBytes[0] <= 32)
			{
			  // fill TX FIFO
			  if (bytesRemain >= writeChunkSize)
			  {
				  // add another 16 bytes to the FIFO
				  strobeVal = BURST_TX_FIFO;
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
				  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
				  HAL_SPI_Transmit(&hspi, &txFifoBuffer[bytesPos], writeChunkSize, 1000); // TX
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  bytesRemain -= writeChunkSize; // subtract from bytes remain
				  bytesPos += writeChunkSize; // add to bytes position
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  HAL_SPIEx_FlushRxFifo(&hspi);
			  }
			  else
			  {
				  // fill TX fifo with last bytes
				  // if 0, break, else fill and break
				  if (bytesRemain == 0)
				  {
					  HAL_SPIEx_FlushRxFifo(&hspi);
					  break;
				  }
				  else
				  {
					  // add remaining bytes to the FIFO
					  strobeVal = BURST_TX_FIFO;
					  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
					  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
					  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
					  HAL_SPI_Transmit(&hspi, &txFifoBuffer[bytesPos], bytesRemain, 1000); // TX
					  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
					  HAL_SPIEx_FlushRxFifo(&hspi);
					  break;
				  }
			  }

			} // end if <= 32

		} // end TX while
		// poll SNOP to get radio status
		// radio drops to IDLE when TX is done
		strobeVal = SNOP;
		while (1)
		{
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
		  HAL_SPI_Receive(&hspi, &cc1101StatusByte, 1, 1000); // RX
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  if ((cc1101StatusByte & 0xF0) == 0)
		  {
			  break;
		  }
		}


	}// end else

	// end TX by: strobeing RX, clearing SPI FIFO, and turning off the LED
	strobeVal = SFRX;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

	strobeVal = SRX;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
	HAL_SPI_Receive(&hspi, &cc1101StatusByte, 1, 1000); // RX
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_SPIEx_FlushRxFifo(&hspi);
	HAL_GPIO_WritePin(TLED_GPIO_Port, TLED_Pin, GPIO_PIN_RESET); // turn off TX LED
} // end CC1101 TX

void cc1101_RX_byte_func(SPI_HandleTypeDef hspi, uint8_t rxBuffer[], uint8_t * rxBufferPos, uint8_t * packetLen, uint8_t * newPacketFlag)
{
	uint8_t strobeVal = 0;
	uint8_t cc1101StatusBytes[2] = {0};

	// if new packet is 1, we need to pull the packet length
	// else we just pull a single byte
	// strobe RXBYTES to get number of bytes in RX FIFO
	// note: due to error in chip design, we can not pull the last byte in the FIFO until the packet is complete
	// if RXBYTES >= 2 pull
	while(1)
	{
		  strobeVal = RXBYTES;
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
		  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
		  HAL_SPI_Receive(&hspi, cc1101StatusBytes, 2, 1000); // RX
		  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);

		  if (cc1101StatusBytes[0] > 1)
		  {
			  if(*newPacketFlag == 1)
			  {
				  // more than 2 bytes in the RXFIFO
				  // first one is the length byte
				  strobeVal = SINGLE_RX_FIFO;
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
				  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
				  HAL_SPI_Receive(&hspi, cc1101StatusBytes, 2, 1000); // RX
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  *packetLen = cc1101StatusBytes[0];
				  // clear new packet flag
				  *newPacketFlag = 0;
				  break;
			  }
			  else
			  {
				  // more than 2 bytes in the RXFIFO
				  strobeVal = SINGLE_RX_FIFO;
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
				  HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
				  HAL_SPI_Receive(&hspi, cc1101StatusBytes, 2, 1000); // RX
				  HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
				  rxBuffer[*rxBufferPos] = cc1101StatusBytes[0];
				  *rxBufferPos += 1;
				  break;
			  }
		  }
	} // end while

}

void cc1101_RX_last_byte_func(SPI_HandleTypeDef hspi, uint8_t rxBuffer[], uint8_t * rxBufferPos, uint8_t * packetLen)
{
	uint8_t strobeVal = 0;
	uint8_t cc1101StatusBytes[2] = {0};

	// just pull the last single byte
	strobeVal = SINGLE_RX_FIFO;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000); // TX
	HAL_SPI_Receive(&hspi, cc1101StatusBytes, 2, 1000); // RX
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	rxBuffer[*rxBufferPos] = cc1101StatusBytes[0];
	*rxBufferPos += 1;

	strobeVal = SIDLE;
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi, &strobeVal, 1, 1000);
	HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET);
}

// globals for USART interrupts
// USART RX buffer variables
uint8_t UartRxByte = 0; // single byte for RX
uint8_t UartRxNewFlag = 0; // flag to signal new byte in buffer
uint8_t UartRxBufferPos = 0; // variable to keep track of RX buffer position
uint8_t UartRxBuffer[MAX_MSG_LENGTH] = {0}; // RX buffer
// RX state machine tracking variables
uint8_t NewRxPacketFlag = 0; // used to indicate this is a new packet, and we need to check for the length
uint8_t RxStateFlag = 0; // used to indicate we are currently in active RX, avoids constantly sending RXBYTES over SPI

int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_TIM16_Init();

  // NOTE: switch to structs next build
  // data storage variables
  // need to buffer:
  // 1. config commands
  // 2. text data
  // use a simple byte header to determine data type:
  // 0x00 - unused
  // 0x01 - configuration command
  // 0x02 - text data

  // USART TX buffer variables
  uint8_t UartTxBuffer[MAX_MSG_LENGTH] = {0}; // storage buffer for UART TX
  uint8_t UartTxBufferPos = 0; // UART TX buffer position
  uint8_t UartTxFlag = 0; // flag to show UART TX should be called
  uint8_t UartTxCount = 0; // UART TX buffer position

  // SPI RX buffer variables
  uint8_t SpiRxBuffer[MAX_MSG_LENGTH] = {0};
  uint8_t SpiRxBufferPos = 0;
  uint8_t SpiRxFlag = 0;

  // SPI TX buffer variables
  uint8_t SpiTxBuffer[MAX_MSG_LENGTH] = {0};
  uint8_t SpiTxFlag = 0;

  // CMD variable
  uint8_t CmdByte = 0; // new channel

  // PA tables
  //uint8_t PAtable_915[] = {0x7E,0x03,0xDE,0x1E,0x27,0x38,0x8E,0x84,0xCC};
  uint8_t PAtable_2FSK[] = {0x7E,0x8E,0xDE,0x1E,0x27,0x38,0x8E,0x84,0xCC};
  uint8_t PAtable_OOK[] = {0x7E,0x03,0x8E,0x1E,0x27,0x38,0x8E,0x84,0xCC};

  // burst FIFO header command always first in the TX SPI buffer
  SpiTxBuffer[0] = BURST_TX_FIFO;

  // timer variables
  uint16_t backoffPeriod = 0; // wait period for backoff for CA
  uint8_t backoffFlag = 0; // need to do CA
  uint8_t timeCount = 0;

  // RX state machine tracking variables
  uint8_t RxPacketLen = 255; // used to indicate the length of the RX packet

  // start our timer
  HAL_TIM_Base_Start(&htim16);

  // flush UART buffer (small bug in some uC where the buffer has a value in it)
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);

  // when reset, we need need to tell the control software to prompt user for reconfiguration
  // set UART TX buffer to [0x03, 0xFF] and pos to 3
  UartTxBuffer[0] = 0x03;
  UartTxBuffer[1] = 0xFF;
  UartTxBufferPos = 2;
  UartTxFlag = 1;

  // USART RX INT start
  HAL_UART_Receive_IT(&huart1, &UartRxByte, 1);

  while (1) // MAIN LOOP
  {
	  /* -------- RX UART SECTION -------- */

	  // if the UART RX "new flag" is set, new message to process
	  if (UartRxNewFlag == 1)
	  {
		  UartRxNewFlag = 0; // set flag to 0
		  switch(UartRxBuffer[0])
		  {
		  case 1: // config command
			  CmdByte = UartRxBuffer[1];
			  backoffPeriod = UartRxBuffer[2] * 1000;
			  if (CmdByte <= 7)
			  {
				  cc1101_init(hspi1, PAtable_2FSK, CmdByte, MDMCFG4_2FSK, MDMCFG2_2FSK, AGCCTRL2_2FSK, AGCCTRL1_2FSK, AGCCTRL0_2FSK, FREND0_2FSK);
			  }
			  else
			  {
				  cc1101_init(hspi1, PAtable_OOK, CmdByte, MDMCFG4_OOK, MDMCFG2_OOK, AGCCTRL2_OOK, AGCCTRL1_OOK, AGCCTRL0_OOK, FREND0_OOK);
			  }
			  break;
		  case 2: // data to radio (from user)
			  SpiTxBuffer[1] = UartRxBufferPos;
			  memcpy(&SpiTxBuffer[2], UartRxBuffer, UartRxBufferPos);
			  SpiTxFlag = 1;
			  break;
		  default:
			  UartRxBufferPos = 0; // position to 0
		  } // end switch-case
		  UartRxBufferPos = 0; // position to 0
	  } // end "new flag" IF

	  /* -------- TX RF SECTION -------- */
	  // check if TX flag is set
	  if (SpiTxFlag == 1)
	  {
		  // TX
		  // basic CA: check if we are in backoff, if so, we dont TX, instead we wait the backoff period (using timers)
		  // once backoff is clear, we check if we can TX, if so, TX
		  // else: we re-wait the backoff (backoff - 1)
		  if (backoffFlag == 0)
		  {
			  // check CCA pin (GDO0). Only TX if clear. Pin high if not currently receiving a packet
			  if(HAL_GPIO_ReadPin(GDO0_GPIO_Port, GDO0_Pin))
			  {
				  // disable interrupts during TX
				  HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
				  cc1101_TX_packet(hspi1, SpiTxBuffer); // TX function
				  // enable interrupts
				  __HAL_GPIO_EXTI_CLEAR_RISING_IT(GDO2_Pin);
				  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
				  SpiTxFlag = 0; // reset TX flag
			  }
			  else // else we set the backoff flag
			  {
				  backoffFlag = 1; // set flag
				  __HAL_TIM_SET_COUNTER(&htim16,0); // reset uS timer
			  }
		  }
		  else
		  {
			  // we are in "backoff" state
			  timeCount = __HAL_TIM_GET_COUNTER(&htim16);
			  if (timeCount > backoffPeriod)
			  {
				  // waited through backoff, set flag to 0
				  backoffFlag = 0; // reset flag
			  }
		  }
	  } // end TX section

	  /* -------- RX RF SECTION -------- */
	  // if we are in RX state, pull from RX FIFO into RX buffer
	  // note: the CC1101 has a RX FIFO bug where we cant pull a byte if RXFIFO is 1 unless it is the last byte
	  // this means we have to do a work-around where we only pull data if the buffer has 2+ bytes (unless it is the last byte)
	  if (RxStateFlag == 1)
	  {
		  // first, we need to check to see if we are at the last byte
		  if(SpiRxBufferPos == (RxPacketLen - 1))
		  {
			  // last byte
			  cc1101_RX_last_byte_func(hspi1, SpiRxBuffer, &SpiRxBufferPos, &RxPacketLen);
			  // /set/reset flags
			  RxPacketLen = 255;
			  RxStateFlag = 0;
			  SpiRxFlag = 1; // set SPI RX Flag
			  // reset to RX
			  cc1101_reset_RX(hspi1);
		  }
		  else
		  {
			  // not last byte, pull single byte from fifo and append to the buffer
			  // advance buffer position by one
			  cc1101_RX_byte_func(hspi1, SpiRxBuffer, &SpiRxBufferPos, &RxPacketLen, &NewRxPacketFlag);
		  }

	  }

	  if(SpiRxFlag == 1)
	  {
		  SpiRxFlag = 0;
		  memcpy(UartTxBuffer, SpiRxBuffer, SpiRxBufferPos);
		  UartTxBufferPos = SpiRxBufferPos;
		  UartTxFlag = 1;
		  SpiRxBufferPos = 0; // position to 0
		  HAL_GPIO_WritePin(RLED_GPIO_Port, RLED_Pin, GPIO_PIN_RESET); // turn off RX LED
	  }

	  // TX text data via UART to user
	  if (UartTxFlag == 1)
	  {
		  if (UartTxCount == UartTxBufferPos)
		  {
			  UartTxFlag = 0;
			  UartTxCount = 0;
			  UartTxBufferPos = 0;
		  }
		  else
		  {
			  HAL_UART_Transmit(&huart1, &UartTxBuffer[UartTxCount], 1, 1000);
			  UartTxCount += 1;
		  }
	  }

  } // END MAIN LOOP

} // END MAIN

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_TIM16_Init(void)
{
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 16-1;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 65535;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 38400;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, CSn_Pin|RLED_Pin|TLED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : GDO0_Pin */
  GPIO_InitStruct.Pin = GDO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GDO0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : CSn_Pin RLED_Pin TLED_Pin */
  GPIO_InitStruct.Pin = CSn_Pin|RLED_Pin|TLED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : GDO2_Pin */
  GPIO_InitStruct.Pin = GDO2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GDO2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	UartRxBuffer[UartRxBufferPos] = UartRxByte; // add to buffer
	UartRxBufferPos += 1; // advance buffer position variable
	if (UartRxByte == 255)
	{
		UartRxNewFlag = 1; // set the flag to show we have a new UART message
	}
	HAL_UART_Receive_IT(&huart1, &UartRxByte, 1); // restart interrupt
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GDO2_Pin && RxStateFlag == 0) // If The INT Source Is EXTI Line 11 (A11) and RX state is 0
    {
    	HAL_GPIO_WritePin(RLED_GPIO_Port, RLED_Pin, GPIO_PIN_SET); // turn on RX LED
    	RxStateFlag = 1; // set RX state flag
    	NewRxPacketFlag = 1; // set new packet flag
    }
}

void Error_Handler(void)
{

  __disable_irq();
  while (1)
  {
  }

}

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{

}
#endif /* USE_FULL_ASSERT */
