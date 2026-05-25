// CC1101 driver implementation
// Author: Richard Shmel

#include "cc1101.h"
#include "main.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1; // Reference to the HAL SPI instance

#define CC1101_SPI_TIMEOUT 10  // 10ms timeout for blocking SPI calls
#define CC1101_FIFO_ADDR 0x3F

#define CS_LOW() HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(CSn_GPIO_Port, CSn_Pin, GPIO_PIN_SET)

// Base configuration (shared across all 3 modes)
// Values calculated using TI SmartRF studio for CC1101
static const uint8_t cc1101_base_config[][2] = {
    {CC1101_IOCFG2,   0x06},
    {CC1101_IOCFG1,   0x2E},
    {CC1101_IOCFG0,   0x09},
    {CC1101_FIFOTHR,  0x47},
    {CC1101_SYNC1,    0xD3},
    {CC1101_SYNC0,    0x91},
    {CC1101_PKTLEN,   0xFF},
    {CC1101_PKTCTRL1, 0x40},
    {CC1101_PKTCTRL0, 0x01},
    {CC1101_ADDR,     0x00},
    {CC1101_FSCTRL1,  0x0F},
    {CC1101_FSCTRL0,  0x00},
    {CC1101_FREQ2,    0x23},
    {CC1101_FREQ1,    0x1D},
    {CC1101_FREQ0,    0x88},
    {CC1101_MDMCFG3,  0x83},
    {CC1101_MDMCFG1,  0x42},
    {CC1101_MDMCFG0,  0xF8},
    {CC1101_DEVIATN,  0x40},
    {CC1101_MCSM2,    0x07},
    {CC1101_MCSM1,    0x30},
    {CC1101_MCSM0,    0x08},
    {CC1101_FOCCFG,   0x16},
    {CC1101_BSCFG,    0x6C},
    {CC1101_AGCCTRL2, 0x43},
    {CC1101_AGCCTRL1, 0x68},
    {CC1101_AGCCTRL0, 0x91},
    {CC1101_WOREVT1,  0x87},
    {CC1101_WOREVT0,  0x6B},
    {CC1101_WORCTRL,  0xFB},
    {CC1101_FREND1,   0x56},
    {CC1101_FSCAL3,   0xE9},
    {CC1101_FSCAL2,   0x2A},
    {CC1101_FSCAL1,   0x00},
    {CC1101_FSCAL0,   0x1F},
    {CC1101_RCCTRL1,  0x41},
    {CC1101_RCCTRL0,  0x00},
    {CC1101_FSTEST,   0x59},
    {CC1101_PTEST,    0x7F},
    {CC1101_AGCTEST,  0x3F},
    {CC1101_TEST2,    0x81},
    {CC1101_TEST1,    0x35},
    {CC1101_TEST0,    0x09}
};

static const uint8_t pa_table_ook[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // OOK null to +11 dBm
static const uint8_t pa_table_fsk[8] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Continuous +11 dBm

void CC1101_WriteReg(uint8_t addr, uint8_t data)
{
    uint8_t tx[2] = {addr, data};
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, tx, 2, CC1101_SPI_TIMEOUT);
    CS_HIGH();
}

uint8_t CC1101_ReadReg(uint8_t addr)
{
    uint8_t tx[2] = {addr | 0x80, 0x00};
    uint8_t rx[2] = {0};
    CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, CC1101_SPI_TIMEOUT);
    CS_HIGH();
    return rx[1];
}

uint8_t CC1101_ReadStatus(uint8_t addr)
{
    uint8_t tx[2] = {addr | 0xC0, 0x00};
    uint8_t rx[2] = {0};
    CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, CC1101_SPI_TIMEOUT);
    CS_HIGH();
    return rx[1];
}

void CC1101_WriteBurst(uint8_t addr, uint8_t* data, uint8_t len)
{
    uint8_t hdr = addr | 0x40;
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &hdr, 1, CC1101_SPI_TIMEOUT);
    HAL_SPI_Transmit(&hspi1, data, len, CC1101_SPI_TIMEOUT);
    CS_HIGH();
}

void CC1101_ReadBurst(uint8_t addr, uint8_t* data, uint8_t len)
{
    uint8_t hdr = addr | 0xC0;
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &hdr, 1, CC1101_SPI_TIMEOUT);
    HAL_SPI_Receive(&hspi1, data, len, CC1101_SPI_TIMEOUT);
    CS_HIGH();
}

uint8_t CC1101_Strobe(uint8_t strobe)
{
    uint8_t rx = 0;
    CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, &strobe, &rx, 1, CC1101_SPI_TIMEOUT);
    CS_HIGH();
    return rx;
}

void CC1101_FullInit(uint8_t mode, uint8_t channel)
{
    CS_LOW();
    HAL_Delay(5);
    CS_HIGH();
    HAL_Delay(5);
    CC1101_Strobe(CC1101_SRES);
    HAL_Delay(5);

    for (uint8_t i = 0; i < (sizeof(cc1101_base_config) / 2); i++)
    {
        CC1101_WriteReg(cc1101_base_config[i][0], cc1101_base_config[i][1]);
    }

    if (mode == 0)
    {
        CC1101_WriteReg(CC1101_MDMCFG4, 0xC7);
        CC1101_WriteReg(CC1101_MDMCFG2, 0x3B);
        CC1101_WriteReg(CC1101_FREND0,  0x11);
        CC1101_WriteBurst(0x3E, (uint8_t*)pa_table_ook, 8);
    }
    else if (mode == 1)
    {
        CC1101_WriteReg(CC1101_MDMCFG4, 0xC8);
        CC1101_WriteReg(CC1101_MDMCFG2, 0x03);
        CC1101_WriteReg(CC1101_FREND0,  0x10);
        CC1101_WriteBurst(0x3E, (uint8_t*)pa_table_fsk, 8);
    }
    else if (mode == 2)
    {
        CC1101_WriteReg(CC1101_MDMCFG4, 0xCA);
        CC1101_WriteReg(CC1101_MDMCFG2, 0x43);
        CC1101_WriteReg(CC1101_FREND0,  0x10);
        // For 4FSK, recommended to alter default AGC val
        CC1101_WriteReg(CC1101_AGCCTRL2, 0x73); // from TI datasheet
        CC1101_WriteBurst(0x3E, (uint8_t*)pa_table_fsk, 8);
    }

    uint8_t hw_channel = (mode * 10) + (channel & 0x0F);
    CC1101_WriteReg(CC1101_CHANNR, hw_channel);

    // Explicit manual PLL calibration before setting to passive RX
    CC1101_Strobe(CC1101_SCAL);
    HAL_Delay(5);

    CC1101_Strobe(CC1101_SRX);
}

int CC1101_Transmit_Packet(uint8_t* payload, uint8_t len)
{
    // Force IDLE and safely flush TX FIFO
    CC1101_Strobe(CC1101_SIDLE);
    CC1101_Strobe(CC1101_SFTX);

    uint32_t timeout = HAL_GetTick();

    if (len <= 63)
    {
        // Single burst load: length byte + entire payload
        uint8_t burst_buf[64];
        burst_buf[0] = len;
        memcpy(&burst_buf[1], payload, len);

        CC1101_WriteBurst(CC1101_FIFO_ADDR, burst_buf, len + 1);
        CC1101_Strobe(CC1101_STX);

        // Poll TXBYTES down to 0 first to prevent PLL abort race condition
        while (1)
        {
            uint8_t txbytes = CC1101_ReadStatus(CC1101_TXBYTES) & 0x7F;
            if (txbytes == 0)
            {
                break;
            }
            if ((HAL_GetTick() - timeout) > 1000)
            {
                CC1101_Strobe(CC1101_SIDLE);
                CC1101_Strobe(CC1101_SFTX);
                CC1101_Strobe(CC1101_SRX);
                return -1;
            }
        }
    }
    else
    {
        // Chunked Loading: length byte + first 63 bytes
        uint8_t burst_buf[64];
        burst_buf[0] = len;
        memcpy(&burst_buf[1], payload, 63);
        CC1101_WriteBurst(CC1101_FIFO_ADDR, burst_buf, 64);

        uint8_t bytes_remain = len - 63;
        uint8_t* ptr = payload + 63;

        CC1101_Strobe(CC1101_STX);

        // Feed FIFO mid-TX
        while (bytes_remain > 0)
        {
            uint8_t txbytes = CC1101_ReadStatus(CC1101_TXBYTES) & 0x7F;

            if (txbytes <= 32)
            {
                uint8_t chunk = (bytes_remain > 32) ? 32 : bytes_remain;
                CC1101_WriteBurst(CC1101_FIFO_ADDR, ptr, chunk);
                bytes_remain -= chunk;
                ptr += chunk;
                timeout = HAL_GetTick(); // Reset timeout while actively loading
            }

            if ((HAL_GetTick() - timeout) > 1000)
            {
                CC1101_Strobe(CC1101_SIDLE);
                CC1101_Strobe(CC1101_SFTX);
                CC1101_Strobe(CC1101_SRX);
                return -1;
            }
        }

        // Poll remaining bytes down to 0
        while (1)
        {
            uint8_t txbytes = CC1101_ReadStatus(CC1101_TXBYTES) & 0x7F;
            if (txbytes == 0)
            {
                break;
            }
            if ((HAL_GetTick() - timeout) > 1000)
            {
                CC1101_Strobe(CC1101_SIDLE);
                CC1101_Strobe(CC1101_SFTX);
                CC1101_Strobe(CC1101_SRX);
                return -1;
            }
        }
    }

    // Verify final drop to IDLE state via MARCSTATE
    timeout = HAL_GetTick();
    while (1)
    {
        uint8_t marcstate = CC1101_ReadStatus(CC1101_MARCSTATE) & 0x1F;
        if (marcstate == 0x01)
        {
            break;
        }
        if ((HAL_GetTick() - timeout) > 100)
        {
            break; // Safe to break if FIFO is already verified empty
        }
    }

    // Return to passive RX
    CC1101_Strobe(CC1101_SRX);
    return 0;
}
