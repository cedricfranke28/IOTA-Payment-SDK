// Copyright 2022 Cedric Franke
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "argtable3/argtable3.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_parser.h"
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"

#include "lcd.h"
#include "led.h"
#include "myWallet.h"
#include "sensor.h"

#include "../components/iota_c/iota_c/src/wallet/bip39.h"
#include "../components/iota_c/iota_c/src/wallet/wallet.h"

static const char *TAG = "mywallet";

iota_wallet_t *mywallet = NULL;

uint64_t latest_balance = 0;

#define APP_WALLET_SEED CONFIG_WALLET_SEED
#define APP_NODE_URL CONFIG_IOTA_NODE_URL
#define APP_NODE_PORT CONFIG_IOTA_NODE_PORT

#if CONFIG_SHOW_BALANCE
#define ENABLE_SHOW_BALANCE 1
#else
#define ENABLE_SHOW_BALANCE 0
#endif

static int endpoint_validation(iota_wallet_t *w) {
  // URL parsing
  struct http_parser_url u;
  char const *const url = APP_NODE_URL;
  http_parser_url_init(&u);
  int parse_ret = http_parser_parse_url(url, strlen(url), 0, &u);
  if (parse_ret != 0) {
    ESP_LOGE(TAG, "invalid URL of the endpoint\n");
    return -1;
  }

  // get hostname
  if (u.field_set & (1 << UF_HOST)) {
    if (sizeof(w->endpoint.host) > u.field_data[UF_HOST].len) {
      strncpy(w->endpoint.host, url + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);
      w->endpoint.host[u.field_data[UF_HOST].len] = '\0';
    } else {
      ESP_LOGE(TAG, "hostname is too long\n");
      return -2;
    }
  }

  // get port number
  if (u.field_set & (1 << UF_PORT)) {
    w->endpoint.port = u.port;
  } else {
    w->endpoint.port = APP_NODE_PORT;
  }

  // TLS?
  if (strncmp(url, "https", strlen("https")) == 0) {
    w->endpoint.use_tls = true;
  } else {
    w->endpoint.use_tls = false;
  }

  return 0;
}

int init_my_Wallet() {
  char ms_buf[256] = {};
  // init mnemonic
  if (strcmp(CONFIG_WALLET_MNEMONIC, "random") == 0) {
    printf("Generating new mnemonic sentence...\n");
    mnemonic_generator(MS_ENTROPY_256, MS_LAN_EN, ms_buf, sizeof(ms_buf));
    printf("###\n%s\n###\n", ms_buf);
    // init wallet with account index 0
    if ((mywallet = wallet_create(ms_buf, "", 0)) == NULL) {
      ESP_LOGE(TAG, "create wallet failed with random mnemonic\n");
      return -1;
    }
  } else {
    if ((mywallet = wallet_create(CONFIG_WALLET_MNEMONIC, "", 0)) == NULL) {
      ESP_LOGE(TAG, "create wallet failed with default mnemonic\n");
      return -1;
    }
  }

  if (endpoint_validation(mywallet) != 0) {
    wallet_destroy(mywallet);
    return -1;
  }

  // config wallet
  ESP_LOGI(TAG, "Connect to node: %s:%d tls:%s", mywallet->endpoint.host, mywallet->endpoint.port,
           mywallet->endpoint.use_tls ? "true" : "false");
  if (wallet_set_endpoint(mywallet, mywallet->endpoint.host, mywallet->endpoint.port, mywallet->endpoint.use_tls) ==
      0) {
    if (wallet_update_bech32HRP(mywallet) != 0) {
      ESP_LOGE(TAG, "update bech32HRP failed");
      wallet_destroy(mywallet);
      return -1;
    }

    ESP_LOGI(TAG, "Bech32HRP: %s", mywallet->bech32HRP);
    return 0;
  }

  ESP_LOGE(TAG, "config endpoint failed");
  wallet_destroy(mywallet);
  return -1;
}

void set_my_LatestBalance() {
  uint64_t balance = 0;
  float lcd_balance = 0;

  if (wallet_balance_by_index(mywallet, 1, 0, &balance) != 0) {
    printf("Err: get balance failed\n");
  }

  latest_balance = balance;
  printf("balance: %" PRIu64 "\n", balance);

#if ENABLE_SHOW_BALANCE
  lcd_balance = balance;

  lcd_print(1, 1, COLOR_MAGENTA, "Balance: %.1f Mi", (lcd_balance / 1000000));
#else
  ESP_LOGE(TAG, "Showing balance is not enabled!");
#endif

  setGreenLEDOn();
  setRedLEDOff();
}

void get_my_Balance() {
  uint64_t balance = 0;
  float lcd_balance = 0;

  if (wallet_balance_by_index(mywallet, 1, 0, &balance) != 0) {
    printf("Err: get balance failed\n");
  }

  if (balance != latest_balance) {
    ESP_LOGI(TAG, "IOTAs were recieved");

    setGreenLEDOff();
    setRedLEDOn();

    latest_balance = balance;

#if ENABLE_SHOW_BALANCE
    lcd_balance = balance;

    lcd_print(1, 1, COLOR_MAGENTA, "Balance: %.1f Mi", (lcd_balance / 1000000));
#else
    ESP_LOGE(TAG, "Showing balance is not enabled!");
#endif

    vTaskDelay(3000 / portTICK_RATE_MS);

    float temp = get_dht_temp();
    vTaskDelay(3000 / portTICK_RATE_MS);

    lcd_print(1, 3, COLOR_MAGENTA, "Temp: %.1f", temp);

    vTaskDelay(5000 / portTICK_RATE_MS);

    lcd_print(1, 3, COLOR_MAGENTA, "Temp: ????");

    setGreenLEDOn();
    setRedLEDOff();
  }
}

void get_my_Address() {
  char tmp_bech32_addr[65];

  wallet_bech32_from_index(mywallet, 1, 0, tmp_bech32_addr);

  ESP_LOGI(TAG, "%s", tmp_bech32_addr);
}

void send_my_IOTA() {
  char msg_id[IOTA_MESSAGE_ID_HEX_BYTES + 1] = {};
  char data[] = "sent from esp32 via iota.c";
  byte_t recv[IOTA_ADDRESS_BYTES] = {};

  char const *const recv_addr = "atoi1qr7knpall5the89tyeg06w03grtd3vh7x7vj479mx3eml94wc2wfvzaq2za";

  address_from_bech32(mywallet->bech32HRP, recv_addr, recv);

  wallet_send(mywallet, 1, 0, recv + 1, 2000000, "ESP32 Wallet", (byte_t *)data, sizeof(data), msg_id, sizeof(msg_id));

  ESP_LOGI(TAG, "Send IOTA to App Wallet");
}