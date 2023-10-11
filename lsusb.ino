// export PICO_TOOLCHAIN_PATH=~/.arduino15/packages/rp2040/tools/pqt-gcc/1.5.0-b-c7bab52/bin/
// export PICO_SDK_PATH=~/.arduino15/packages/rp2040/hardware/rp2040/3.1.1/pico-sdk


#include "lsusb.host.h"

#define HOST_PIN_DP 2 // Pin used as D+ for host, D- = D+ + 1


// core1: handle host events
void setup1() {
  //sleep_ms(10);
  while ( !Serial ) delay(10);   // wait for native usb
  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = HOST_PIN_DP;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  printf("Core1 setup to run TinyUSB host\n");
  printf("Loaded %d vendor ids and %d product ids\n", usb_vids_count, usb_pids_count);

  // Check for CPU frequency, must be multiple of 120Mhz for bit-banging USB
  uint32_t cpu_hz = clock_get_hz(clk_sys);
  if ( cpu_hz != 120000000UL && cpu_hz != 240000000UL ) {
    while ( !Serial ) delay(10);   // wait for native usb
    printf("Error: CPU Clock = %lu, PIO USB require CPU clock must be multiple of 120 Mhz\r\n", cpu_hz);
    printf("Change your CPU Clock to either 120 or 240 Mhz in Menu->CPU Speed \r\n");
    while(1) delay(1);
  }
  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  tuh_init(1);

  printf("USB Host ready");

  // while (true) {
  //   tuh_task(); // tinyusb host task
  // }
}

void loop1()
{
  tuh_task(); // tinyusb host task
  //sleep_ms(10);
}


// core0: handle device events
void setup() {
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  //set_sys_clock_khz(120000, true);
  Serial1.begin(115200);
  Serial.begin(115200);

  //sleep_ms(5000);
  while ( !Serial ) delay(10);   // wait for native usb
  //printf("Core0 setup to run TinyUSB device\n");

  //multicore_reset_core1();
  // all USB task run in core1
  //multicore_launch_core1(setup1);
  // init device stack on native usb (roothub port0)
  //tud_init(0);
  // while (true) {
  //   tud_task(); // tinyusb device task
  //   tud_cdc_write_flush();
  // }
  //
  // return 0;
}


void loop()
{
  //tud_task(); // tinyusb device task
  //tud_cdc_write_flush();
}

