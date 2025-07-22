/**
 * @file lt_test_rev_ping.c
 * @brief Test function which tries Ping command with max length of message with pairing slot 0
 * @author Tropic Square s.r.o.
 *
 * @license For the license see file LICENSE.txt file in the root directory of this source tree.
 */

#include "inttypes.h"
#include "libtropic.h"
#include "libtropic_common.h"
#include "libtropic_functional_tests.h"
#include "libtropic_logging.h"
#include "string.h"

// Ping message of max length. For this example LT_SIZE_OF_L3_BUFF must be set to L3_FRAME_MAX_SIZE,
// Check libtropic_common.h for more details
#define PING_LEN PING_LEN_MAX

/**
 * @brief
 *
 * @return int
 */
int lt_test_rev_ping(void)
{
    LT_LOG(
        "  "
        "------------------------------------------------------------------------------------------------------------"
        "-");
    LT_LOG(
        "  -------- lt_test_rev_ping() "
        "-------------------------------------------------------------------------------------");
    LT_LOG(
        "  "
        "------------------------------------------------------------------------------------------------------------"
        "-");

    lt_handle_t h = {0};
#if LT_SEPARATE_L3_BUFF
    uint8_t l3_buffer[LT_SIZE_OF_L3_BUFF] __attribute__((aligned(16))) = {0};
    h.l3.buff = l3_buffer;
    h.l3.buff_len = sizeof(l3_buffer);
#endif
    uint8_t ping_msg[PING_LEN];
    // Ping message definition
    for (uint8_t i = 0; i < (PING_LEN / 32); i++) {
        memcpy(ping_msg + 32 * i, "This is ping message to be sent", 32);
    }

    uint8_t in[PING_LEN] = {0};

    LT_LOG("%s", "Initialize handle");
    LT_TEST_ASSERT(LT_OK, lt_init(&h));

    // Ping with SH0
    LT_LOG("%s with %d", "verify_chip_and_start_secure_session()", PAIRING_KEY_SLOT_INDEX_0);
    LT_TEST_ASSERT(LT_OK, verify_chip_and_start_secure_session(&h, sh0priv, sh0pub, PAIRING_KEY_SLOT_INDEX_0));
    LT_LOG("%s", "lt_ping() ");
    LT_TEST_ASSERT(LT_OK, lt_ping(&h, ping_msg, in, PING_LEN));
    LT_LOG("Asserting %d B of Ping message", PING_LEN);
    LT_TEST_ASSERT(0, memcmp(in, ping_msg, PING_LEN));
    LT_LOG_LINE();
    LT_LOG("%s", "lt_session_abort()");
    LT_TEST_ASSERT(LT_OK, lt_session_abort(&h));
    memset(in, 0x00, PING_LEN);

    // Deinit handle
    LT_LOG("%s", "lt_deinit()");
    LT_TEST_ASSERT(LT_OK, lt_deinit(&h));

    return 0;
}
