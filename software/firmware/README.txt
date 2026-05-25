Recommend using STM32 CubeIDE. The two source files “main.c” and “cc1101.c” go in the Src directory. The two header files “main.h” and “cc1101.h” go in the Inc directory.

Note: Depending on the interrupt configuration you may need to modify the interrupt handler in the “stm32xx_it.c” file for your specific microcontroller.

You might need to change the USART1 IRQ handler function to enable UART DMA idle line interrupts as shown below:

void USART1_IRQHandler(void)
{
  if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
      __HAL_UART_CLEAR_IDLEFLAG(&huart1);
      AirChat_UART_IDLE_Callback();
  }
  HAL_UART_IRQHandler(&huart1);
}

