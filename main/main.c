// Copyright 2021 IOTA Stiftung
// SPDX-License-Identifier: Apache-2.0

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

// console system
#include "argtable3/argtable3.h"
#include "driver/uart.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
// sntp
#include "esp_sntp.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "cli_wallet.h"
#include "sensor.h"
#include "lcd.h"
#include "myWallet.h"
#include "led.h"

#define APP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define APP_WIFI_PWD CONFIG_ESP_WIFI_PASSWORD
#define APP_WIFI_RETRY CONFIG_ESP_MAXIMUM_RETRY

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char* TAG = "main";
static int s_retry_num = 0;


// reboot function, to reboot the system
static void reboot() {
  for (int i = 20; i >= 0; i--) {
    printf("Restarting in %d seconds...\n", i);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  printf("Restarting now.\n");

  fflush(stdout);
  esp_restart();
}

//eventhandler, handels the wifi connection
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < APP_WIFI_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP: %s", APP_WIFI_SSID);
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

//wifi init, initialize wifi parameters
void wifi_init(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = APP_WIFI_SSID,
              .password = APP_WIFI_PWD,
              /* Setting a password implies station will connect to all security modes including WEP/WPA.
               * However these modes are deprecated and not advisable to be used. Incase your Access point
               * doesn't support WPA2, these mode can be enabled by commenting below line */
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,

              .pmf_cfg = {.capable = true, .required = false},
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits =
      xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", APP_WIFI_SSID);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", APP_WIFI_SSID, APP_WIFI_PWD);
    reboot();
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  vEventGroupDelete(s_wifi_event_group);
}

//initialize_nvs, initialize Non-volatile storage
static void initialize_nvs() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

//dump chipinfo, outputs information about chip, wifi, esp-idf
static void dump_chip_info() {
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU core(s), WiFi%s%s, ", CONFIG_IDF_TARGET, chip_info.cores,
         (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
  printf("silicon revision %d, ", chip_info.revision);
  printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
  printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
  printf("esp-idf version: %s, app_version: %s\n", esp_get_idf_version(), APP_WALLET_VERSION);
}

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void) {
  static wl_handle_t wl_handle;
  const esp_vfs_fat_mount_config_t mount_config = {.max_files = 4, .format_if_mount_failed = true};
  esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
    return;
  }
}
#endif  // CONFIG_STORE_HISTORY

//update time, to get the current time from timezone
static void update_time() {
  // init sntp
  ESP_LOGI(TAG, "Initializing SNTP: %s, Timezone: %s", CONFIG_SNTP_SERVER, CONFIG_SNTP_TZ);
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, CONFIG_SNTP_SERVER);
  // sntp_setservername(0, "pool.ntp.org");
  sntp_init();

  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 10;
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  time(&now);

  // set timezone
  char strftime_buf[64] = {};
  setenv("TZ", CONFIG_SNTP_TZ, 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time is: %s (%" PRIu64 ")", strftime_buf, timestamp());
}

void app_main(void) {
  // Initialize NVS
  initialize_nvs();

  // Print chip information
  dump_chip_info();
  
  //lcd panel
  lcd_init();

  //lcd fill screen
  lcd_fill_screen(COLOR_WHITE);

  //lcd print 
  lcd_print(1, 1, COLOR_BLACK, "Init LCD-Panel...");

  // wifi setup
  wifi_init();

  //lcd print Wifi SSID
  lcd_print(1, 1, COLOR_BLACK, "SSID: %s ...", CONFIG_ESP_WIFI_SSID);

  // temperature sensor
  init_tempsensor();

  //init DHT22
  init_dht_tempsensor();

  //init LED Green
  initGreenLED();

  //init LED Red
  initRedLED();

  // get time from sntp
  update_time();

  //lcd print Inititialize Wallet
  lcd_print(1, 1, COLOR_BLACK, "Init Wallet...");

  // init my Wallet, to initialize Wallet from myWallet.h
  if (init_my_Wallet()) {
    ESP_LOGE(TAG, "Init wallet instance failed\n");
    lcd_print(1, 10, COLOR_RED, "Initial client failed");
    lcd_print(1, 12, COLOR_RED, "Restart in 5s");
    reboot();
  }
  
  //lcd print Wallet Address
  lcd_print(1,1,COLOR_RED, "Wallet Address    " );

  //lcd display the qr-code with included address
  lcd_display_qr();

  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
  {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // ESP log level
  esp_log_level_set("*", ESP_LOG_INFO);

  // lcd print Temp "????" means unvailable
  lcd_print(1, 3, COLOR_MAGENTA, "Temp: ????");

  // get my Address from myWallet.h 
  get_my_Address();

  //uncommend to send IOTA. ! Config reciver Address in myWallet.h
  //send_my_IOTA();
  
  //set my latest balance, to get the current balance
  set_my_LatestBalance();

  while(1)
  {
    //get my Balance, to request the current balance and get an payment.
    get_my_Balance();
  }
  reboot();
}
