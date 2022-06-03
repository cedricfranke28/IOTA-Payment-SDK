#[[
Copyright 2020 IOTA Stiftung
SPDX-License-Identifier: Apache-2.0
]]

if(CONFIG_IOTA_UNIT_TESTS)
  idf_component_register(SRCS "../test/test_main.c" INCLUDE_DIRS ".")
elseif(CONFIG_ENABLE_LCD)
  idf_component_register(SRCS "main.c" "cli_wallet.c" "myWallet.c" "sensor.c" "events_api.c" "lcd.c" "led.c"  INCLUDE_DIRS "." PRIV_REQUIRES ST7735 qrcodegen)

else()
  idf_component_register(SRCS "main.c" "cli_wallet.c" "sensor.c" "events_api.c" INCLUDE_DIRS
                         ".")
endif()

