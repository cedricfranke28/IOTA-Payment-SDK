#pragma once

#define WALLET_VERSION_MAJOR 0
#define WALLET_VERSION_MINOR 4
#define WALLET_VERSION_MICRO 0

#define VER_STR0(s) #s
#define VER_STR(s) VER_STR0(s)

#define APP_WALLET_VERSION      \
  VER_STR(WALLET_VERSION_MAJOR) \
  "." VER_STR(WALLET_VERSION_MINOR) "." VER_STR(WALLET_VERSION_MICRO)

//Funktions
  int init_my_Wallet();
  void set_my_LatestBalance();
  void get_my_Balance();
  void get_my_Address();
  void send_my_IOTA();
