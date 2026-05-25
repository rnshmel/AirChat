// AirChat PCB main firmware
// Author: Richard Shmel

#include "main.h"
#include <string.h>
#include "cc1101.h"

typedef struct
{
    uint8_t message_in_slot; // 0 = empty, 1 = populated
    uint8_t message_type; // 0x01 = gonfig, 0x02 = chat msg, 0x03 = status
    uint8_t payload_len;
    uint8_t payload[256];
} PacketBuffer;

typedef enum
{
    STATE_IDLE,
    STATE_PASSIVE_RX,
    STATE_ACTIVE_RX,
    STATE_ACTIVE_TX
} SystemState_t;

#define UART_BUF_SIZE 256
#define RING_BUF_SIZE 4

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim16;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

volatile SystemState_t current_state = STATE_IDLE;
volatile uint8_t current_rf_expected_len = 0;

uint8_t uart_rx_dma_buf[UART_BUF_SIZE];
uint8_t uart_tx_dma_buf[UART_BUF_SIZE];

PacketBuffer uart_rx_ring[RING_BUF_SIZE];
volatile uint8_t uart_rx_head = 0;
volatile uint8_t uart_rx_tail = 0;

PacketBuffer cc1101_rx_ring[RING_BUF_SIZE];
volatile uint8_t cc1101_rx_head = 0;
volatile uint8_t cc1101_rx_tail = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM16_Init(void);
static void MX_USART1_UART_Init(void);

void AirChat_UART_IDLE_Callback(void);
void Send_UART_Status(uint8_t status_id, uint8_t code);
void Transmit_UART_DMA(uint8_t* data, uint16_t len);

void Execute_Synchronous_Config(uint8_t* payload);
int Execute_Synchronous_TX(uint8_t* payload, uint16_t len);

// Main function
int main(void)
{
  // Reset of all peripherals, Initializes the Flash interface and the Systick.
  HAL_Init();

  // Configure the system clock
  SystemClock_Config();

  // Initialize all configured peripherals
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM16_Init();
  MX_USART1_UART_Init();

  // Enable UART IDLE interrupt
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

  // Start UART DMA RX
  HAL_UART_Receive_DMA(&huart1, uart_rx_dma_buf, UART_BUF_SIZE);

  // Start Timer 16
  HAL_TIM_Base_Start_IT(&htim16);

  // Send initial status: hardware reset (or init boot)
  Send_UART_Status(0x03, 0x00);
  current_state = STATE_PASSIVE_RX;

  // Mian inf loop
  while (1)
  {
    // Route RF packets to host computer
    if (cc1101_rx_head != cc1101_rx_tail)
    {
        PacketBuffer *rf_pkt = &cc1101_rx_ring[cc1101_rx_tail];

        // Data coming from CC1101 already starts with 0x02
        Transmit_UART_DMA(rf_pkt->payload, rf_pkt->payload_len);

        rf_pkt->message_in_slot = 0;
        cc1101_rx_tail = (cc1101_rx_tail + 1) % RING_BUF_SIZE;
    } // end route

    // Process commands from computer
    if (uart_rx_head != uart_rx_tail)
    {
        PacketBuffer *host_pkt = &uart_rx_ring[uart_rx_tail];

        if (host_pkt->message_type == 0x01)
        {
            Execute_Synchronous_Config(host_pkt->payload);
            uart_rx_tail = (uart_rx_tail + 1) % RING_BUF_SIZE;
        }
        else if (host_pkt->message_type == 0x02)
        {
        	// If STATE_ACTIVE_RX, do nothing. It stays in the ring buffer for the next loop.
            if (current_state != STATE_ACTIVE_RX)
            {
                int tx_status = Execute_Synchronous_TX(host_pkt->payload, host_pkt->payload_len);

                if (tx_status == 0)
                {
                    Send_UART_Status(0x03, 0x01); // TX complete ACK response
                }
                else
                {
                    Send_UART_Status(0x03, 0xFF); // Hardware Error
                }
                uart_rx_tail = (uart_rx_tail + 1) % RING_BUF_SIZE;
            }
        }
        else
        {
            // Unknown packet type, ignore
            uart_rx_tail = (uart_rx_tail + 1) % RING_BUF_SIZE;
        }

    } // end cmd proc

  } // end inf loop

} // end main

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

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
  htim16.Init.Prescaler = 159;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 250;
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
  huart1.Init.BaudRate = 115200;
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

static void MX_DMA_Init(void)
{

  // DMA controller clock enable
  __HAL_RCC_DMA1_CLK_ENABLE();

  // DMA1_Channel1_IRQn interrupt configuration
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

  // DMA1_Channel2_3_IRQn interrupt configuration
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, CSn_Pin|RX_LED_Pin|TX_LED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIO0_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = CSn_Pin|RX_LED_Pin|TX_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIO1_GPIO_Port, &GPIO_InitStruct);

}

void AirChat_UART_IDLE_Callback(void)
{
    // Calculate bytes received
    uint16_t len = UART_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);

    // Stop DMA
    HAL_UART_DMAStop(&huart1);

    // Extract to ring buffer if valid
    if (len > 0 && len <= UART_BUF_SIZE)
    {
        PacketBuffer *pkt = &uart_rx_ring[uart_rx_head];
        pkt->message_type = uart_rx_dma_buf[0];
        pkt->payload_len = (uint8_t)len;
        memcpy(pkt->payload, uart_rx_dma_buf, len);
        pkt->message_in_slot = 1;

        uart_rx_head = (uart_rx_head + 1) % RING_BUF_SIZE;
    }

    // Restart DMA for next packet
    HAL_UART_Receive_DMA(&huart1, uart_rx_dma_buf, UART_BUF_SIZE);
}

void Send_UART_Status(uint8_t status_id, uint8_t code)
{
    while (huart1.gState != HAL_UART_STATE_READY) {} // Wait for previous TX

    uart_tx_dma_buf[0] = status_id;
    uart_tx_dma_buf[1] = code;

    HAL_UART_Transmit_DMA(&huart1, uart_tx_dma_buf, 2);
}

void Transmit_UART_DMA(uint8_t* data, uint16_t len)
{
    if (len > 256) len = 256;

    while (huart1.gState != HAL_UART_STATE_READY) {} // Wait for previous TX

    memcpy(uart_tx_dma_buf, data, len);
    HAL_UART_Transmit_DMA(&huart1, uart_tx_dma_buf, len);
}

void Execute_Synchronous_Config(uint8_t* payload)
{
    // Lock out Timer 16 SPI polling to prevent bus collisions
	// Note: next update add a "config state"
    current_state = STATE_IDLE;
    HAL_GPIO_WritePin(GPIOA, RX_LED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, TX_LED_Pin, GPIO_PIN_SET);

    uint8_t channel = payload[1];
    uint8_t mode = payload[2];

    // Perform full config via SPI
    CC1101_FullInit(mode, channel);

    // Adjust timer 16 interval based on mode
    // Clock is 10us per tick
    // This sets how often we poll RX BYTES
    uint32_t timer_period = 300; // Default = mode 1 (standard)
    if (mode == 0) timer_period = 600;      // Legacy (4800 baud) = 600 ms
    else if (mode == 1) timer_period = 300; // Standard (9600 baud) = 300 ms
    else if (mode == 2) timer_period = 100;  // Advanced (38400 baud) = 100 ms

    __HAL_TIM_SET_AUTORELOAD(&htim16, timer_period);

    // Config complete, respond back to host computer
    Send_UART_Status(0x03, 0x03);

    // Release SPI bus back to the timer poll interrupt
    HAL_GPIO_WritePin(GPIOA, RX_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, TX_LED_Pin, GPIO_PIN_RESET);
    current_state = STATE_PASSIVE_RX;
}

int Execute_Synchronous_TX(uint8_t* payload, uint16_t len)
{
    // Lock out timer SPI polling
    current_state = STATE_ACTIVE_TX;
    HAL_GPIO_WritePin(GPIOA, TX_LED_Pin, GPIO_PIN_SET);

    // Call our TX burst function
    int status = CC1101_Transmit_Packet(payload, (uint8_t)len);

    // Release SPI bus back to polling
    HAL_GPIO_WritePin(GPIOA, TX_LED_Pin, GPIO_PIN_RESET);
    current_state = STATE_PASSIVE_RX;

    return status;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM16)
    {
        static uint16_t rx_bytes_received = 0;
        static uint8_t last_rxbytes = 0;
        static uint8_t watchdog_counter = 0;

        if (current_state == STATE_PASSIVE_RX)
        {
            uint8_t rxbytes = CC1101_ReadStatus(CC1101_RXBYTES) & 0x7F;

            if (rxbytes > 0)
            {
                // A new packet has started arriving
                PacketBuffer *pkt = &cc1101_rx_ring[cc1101_rx_head];

                // Read length byte (First byte in the FIFO)
                uint8_t len_byte = 0;
                CC1101_ReadBurst(0x3F, &len_byte, 1);

                // Drop impossible packet lengths
                if (len_byte == 0 || len_byte > 250)
                {
                    CC1101_Strobe(CC1101_SIDLE);
                    CC1101_Strobe(CC1101_SFRX);
                    CC1101_Strobe(CC1101_SRX);
                    return;
                }

                current_rf_expected_len = len_byte;
                pkt->payload_len = len_byte;

                rx_bytes_received = 0;
                last_rxbytes = 0;
                watchdog_counter = 0;

                HAL_GPIO_WritePin(GPIOA, RX_LED_Pin, GPIO_PIN_SET);
                current_state = STATE_ACTIVE_RX;
            }
        }
        else if (current_state == STATE_ACTIVE_RX)
        {
            uint8_t rxbytes = CC1101_ReadStatus(CC1101_RXBYTES) & 0x7F;
            PacketBuffer *pkt = &cc1101_rx_ring[cc1101_rx_head];

            if (rxbytes > 0)
            {
                uint8_t bytes_to_read = 0;

                // If the remaining bytes are fully in the FIFO, read them all
                if ((rx_bytes_received + rxbytes) >= current_rf_expected_len)
                {
                    bytes_to_read = current_rf_expected_len - rx_bytes_received;
                }
                // Otherwise, read what we can, but leave 1 byte to prevent the CC1101 RX FIFO underflow bug
                else if (rxbytes > 1)
                {
                    bytes_to_read = rxbytes - 1;
                }

                if (bytes_to_read > 0)
                {
                    CC1101_ReadBurst(0x3F, &pkt->payload[rx_bytes_received], bytes_to_read);
                    rx_bytes_received += bytes_to_read;
                    watchdog_counter = 0; // Reset watchdog
                }
            }

            // Check if packet int is complete
            if (rx_bytes_received >= current_rf_expected_len)
            {
                pkt->message_type = pkt->payload[0]; // ID, will always be 0x02 from the CC1101
                pkt->message_in_slot = 1;
                cc1101_rx_head = (cc1101_rx_head + 1) % RING_BUF_SIZE;

                // Because MCSM1=0x30, radio is now in IDLE.
                // Reset it back up to passive RX
                CC1101_Strobe(CC1101_SRX);
                HAL_GPIO_WritePin(GPIOA, RX_LED_Pin, GPIO_PIN_RESET);
                current_state = STATE_PASSIVE_RX;
            }
            else
            {
                // Watchdog: If rxbytes hasn't grown in multiple ticks, we dropped a byte in RX
                if (rxbytes == last_rxbytes)
                {
                    watchdog_counter++;
                    // Hard-code to 5 for now
                    if (watchdog_counter > 5)
                    {
                        // Hardware recovery: force IDLE, flush FIFO, and restart RX
                        CC1101_Strobe(CC1101_SIDLE);
                        CC1101_Strobe(CC1101_SFRX);
                        CC1101_Strobe(CC1101_SRX);
                        HAL_GPIO_WritePin(GPIOA, RX_LED_Pin, GPIO_PIN_RESET);
                        current_state = STATE_PASSIVE_RX;
                    }
                }
                else
                {
                    last_rxbytes = rxbytes;
                    watchdog_counter = 0;
                }
            }
        }
    }
}

void Error_Handler(void)
{

  __disable_irq();
  while (1)
  {
  }

}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{

}
#endif
