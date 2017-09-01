// user_main.c
// Copyright 2017 Lukas Friedrichsen
// License: Apache License Version 2.0
//
// 2017-05-16

#include "mem.h"
#include "osapi.h"
#include "user_interface.h"
#include "user_config.h"

// Definition of functions (so there won't be any complications because the
// compiler resolves the scope top-down):

// Timer- and interrupt-handler-functions:
static void button_pressed_interrupt_handler(void *arg);
static void button_released_interrupt_handler(void *arg);
static void led_blink_timerfunc(void *arg);

// GPIO control:
static void status_led_on(void);
static void status_led_off(void);
void output_power_on(void);
void output_power_off(void);

// Initialization and configuration:
static void gpio_pins_init(void);

void user_init(void);

// Radio frequency configuration
uint32 user_rf_cal_sector_set(void);
void user_rf_pre_init(void);

/*------------------------------------*/

// Declaration and initialization of variables:

static uint32_t actuation_start_time = 0;
static uint8_t mesh_enable_attempt_count = 0;

static os_timer_t *led_blink_timer = NULL;

/*------------------------------------*/

// Timer- and interrupt-handler-functions:

// Interrupt-handler-function, that is called on the actuation of the pushbutton; store the start-time of the actuation
// and re-configure the interrupt to be triggered again on the button's release
static void ICACHE_FLASH_ATTR button_pressed_interrupt_handler(void *arg) {
  os_printf("button_pressed_interrupt_handler: Button actuated!\n");

  // Store the start-time of the actuation
  actuation_start_time = system_get_time();

  // Turn on the status-LED for the duration of the button's actuation
  status_led_on();

  // Re-configure the interrupt to be triggered again on the button's release
  ETS_GPIO_INTR_DISABLE();  // Disable interrupts before changing the current configuration
  ETS_GPIO_INTR_ATTACH(button_released_interrupt_handler, GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO)); // Attach the corresponding function to be executed to the interrupt-pin
  gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO), GPIO_PIN_INTR_POSEDGE); // Configure the interrupt-function to be executed on a positive edge
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(BUTTON_INTERRUPT_GPIO));  // Clear the interrupt-mask
  ETS_GPIO_INTR_ENABLE(); // Re-enable the interrupts
}

// Interrupt-handler-function, that is called on the release of the pushbutton; set the node's operation-mode depending
// on the button's actuation-time and initialize the mesh-network
static void ICACHE_FLASH_ATTR button_released_interrupt_handler(void *arg) {
  os_printf("button_released_interrupt_handler: Button released!\n");

  // Determine, how long the button has been actuated
  uint32_t actuation_time = (system_get_time()-actuation_start_time)/1000;  // Divided by 1000 because system_get_time returns the current time in µs and not in ms

  // Set the node's operation-mode depending on the actuation-time; if the actuation-time is longer than the defined
  // threshold (cf. OPERATION_MODE_THRESHOLD), the operation-mode is set to root-node, otherwise it is set to sub-node
  if (actuation_time > OPERATION_MODE_THRESHOLD) {
    os_printf("button_released_interrupt_handler: Setting device's operation mode to root-node!\n");
  }
  else {
    os_printf("button_released_interrupt_handler: Setting device's operation mode to sub-node!\n");
  }

  // Turn the status-LED back off
  status_led_off();

  // Reset the actuation-time
  actuation_time = 0;

  // Re-configure the interrupt to be triggered again on the button's actuation
  /*ETS_GPIO_INTR_DISABLE();  // Disable interrupts before changing the current configuration
  ETS_GPIO_INTR_ATTACH(button_pressed_interrupt_handler, GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO)); // Attach the corresponding function to be executed to the interrupt-pin
  gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO), GPIO_PIN_INTR_NEGEDGE); // Configure the interrupt-function to be executed on a positive edge
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(BUTTON_INTERRUPT_GPIO));  // Clear the interrupt-mask
  ETS_GPIO_INTR_ENABLE(); // Re-enable the interrupts*/

  // Disable the interrupt (it is just meant to initialize the mesh-network if it's disabled, not to change
  // the operation-mode whilst already running since that might cause problems in the networks infrastructure)
  ETS_GPIO_INTR_DISABLE();
}

// Timer-function, that toggles the status-LED
static void ICACHE_FLASH_ATTR led_blink_timerfunc(void *arg) {
  // Get the current state of the status-LED
  if (!(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT(STATUS_LED_GPIO))) {
    // Set the output-value to high
    status_led_off();
  }
  else {
    // Set the output-value to low
    status_led_on();
  }
}

/*------------------------------------*/

// GPIO control:

// Switch the status-LED on and set it the corresponding pin to output-mode
static void ICACHE_FLASH_ATTR status_led_on(void) {
  gpio_output_set(0, BIT(STATUS_LED_GPIO), BIT(STATUS_LED_GPIO), 0);
}

// Switch the status-LED off and set it the corresponding pin to output-mode
static void ICACHE_FLASH_ATTR status_led_off(void) {
  gpio_output_set(BIT(STATUS_LED_GPIO), 0, BIT(STATUS_LED_GPIO), 0);
}

// Turn the smart plug's output power and the red LED on
void ICACHE_FLASH_ATTR output_power_on(void) {
  gpio_output_set(BIT(OUTPUT_POWER_RELAY_GPIO), 0, BIT(OUTPUT_POWER_RELAY_GPIO), 0);
}

// Turn the smart plug's output power and the red LED off
void ICACHE_FLASH_ATTR output_power_off(void) {
  gpio_output_set(0, BIT(OUTPUT_POWER_RELAY_GPIO), BIT(OUTPUT_POWER_RELAY_GPIO), 0);
}

/*------------------------------------*/

// Initialization and configuration:

// Initialize the GPIO-pins to function as intended
static void ICACHE_FLASH_ATTR gpio_pins_init(void) {
  os_printf("gpio_pins_init: Initializing GPIO-pins!\n");

  // Initialize the GPIO-subsystem
  gpio_init();

  // Set the defined pins' operation-mode to GPIO
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);

  // Enable the pull-up resistor of the status-LED-GPIO (since the LED is connected in reverse logic); that's
  // also why status_led_on "disables" the pin to turn on the light (resp. the other way round for status_led_off)
  PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);

  // Set the status-LED's GPIO-pin to output-mode and deactivate it
  /*led_blink_timer = (os_timer_t *) os_zalloc(sizeof(os_timer_t));
  if (!led_blink_timer) {
    os_printf("gpio_pins_init: Failed to initialize led_blink_timer!\n");
    return;
  }
  os_timer_disarm(led_blink_timer);
  os_timer_setfn(led_blink_timer, (os_timer_func_t *) led_blink_timerfunc, NULL);
  os_timer_arm(led_blink_timer, LED_BLINK_INTERVAL_LONG, 1);*/
  status_led_off();

  // Set the output-power-relay's GPIO-pin to output-mode and energize it by default, so that the outlet, which the
  // smart plug is connected to, isn't blocked and can still be used as long as the relay isn't turned off per command
  // (cf. mesh_bin.c)
  output_power_on();

  // Set the pushbutton's GPIO-pin to input-mode
  gpio_output_set(0, 0, 0, BIT(BUTTON_INTERRUPT_GPIO));

  // Initialize the pushbutton-pin to function as an interrupt
  ETS_GPIO_INTR_DISABLE();  // Disable interrupts before changing the current configuration
  ETS_GPIO_INTR_ATTACH(button_pressed_interrupt_handler, GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO)); // Attach the corresponding function to be executed to the interrupt-pin
  gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_INTERRUPT_GPIO), GPIO_PIN_INTR_NEGEDGE); // Configure the interrupt-function to be executed on a positive edge
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(BUTTON_INTERRUPT_GPIO));  // Clear the interrupt-mask (otherwise, the interrupt might be masked because of random initialization-values in the corresponding interrupt-register)
  ETS_GPIO_INTR_ENABLE(); // Re-enable the interrupts
}

// Entry point in the program; start the initialization-process
void user_init(void) {
  os_printf("user_init: Starting the initialization-process!\n");

  wifi_station_disconnect();

  // Initialize the GPIO-pins
  gpio_pins_init();
}

/*------------------------------------*/

// Radio frequency configuration:

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABBBCDDD
 *                A : rf cal
 *                B : at parameters
 *                C : rf init data
 *                D : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
      case FLASH_SIZE_4M_MAP_256_256:
        rf_cal_sec = 128 - 8;
        break;

      case FLASH_SIZE_8M_MAP_512_512:
        rf_cal_sec = 256 - 5;
        break;

      case FLASH_SIZE_16M_MAP_512_512:
      case FLASH_SIZE_16M_MAP_1024_1024:
        rf_cal_sec = 512 - 5;
        break;

      case FLASH_SIZE_32M_MAP_512_512:
      case FLASH_SIZE_32M_MAP_1024_1024:
        rf_cal_sec = 1024 - 5;
        break;

      default:
        rf_cal_sec = 0;
        break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void) {
  // Nothing to do...
}
