// Copyright 2020 IOTA Stiftung
// SPDX-License-Identifier: Apache-2.0

/**
 * @brief A simple example for checking balances.
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/utils/byte_buffer.h"
#include "wallet/wallet.h"

#define NODE_HOST "api.lb-0.h.chrysalis-devnet.iota.cafe"
#define NODE_HOST_PORT 443
#define NODE_USE_TLS true

// replace this with your mnemonic string
static char const *const test_mnemonic = "your_testing_mnemonic_sentence";

int main(int argc, char *argv[]) {
  iota_wallet_t *wallet = wallet_create(test_mnemonic, "", 0);
  char tmp_bech32[BECH32_ADDRESS_LEN] = {};
  if (wallet) {
    wallet_set_endpoint(wallet, NODE_HOST, NODE_HOST_PORT, NODE_USE_TLS);
    if (wallet_update_bech32HRP(wallet) != 0) {
      wallet_destroy(wallet);
      printf("Connect Node failed\n");
      return -1;
    }

    // get balance from first 5 addresses
    for (uint32_t idx = 0; idx < 5; idx++) {
      uint64_t value = 0;
      // get balance from a given index
      if (wallet_balance_by_index(wallet, false, idx, &value) != 0) {
        printf("wallet get balance from address index [%" PRIu32 "]failed\n", idx);
        goto done;
      }
      // get bech32 address from index
      wallet_bech32_from_index(wallet, false, idx, tmp_bech32);
      printf("[%" PRIu32 "] %s: %" PRIu64 "\n", idx, tmp_bech32, value);
    }
  } else {
    printf("Create wallet failed\n");
    return -1;
  }

done:
  wallet_destroy(wallet);
  return 0;
}
