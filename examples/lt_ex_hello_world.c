/**
 * @file TROPIC01_hw_wallet.c
 * @brief Example usage of TROPIC01 chip in a generic *hardware wallet* project.
 * @author Tropic Square s.r.o.
 *
 * @license For the license see file LICENSE.txt file in the root directory of this source tree.
 */

#include "string.h"

#include "libtropic.h"
#include "libtropic_common.h"
#include "libtropic_examples.h"

/**
 * @name Hello World
 * @note We recommend reading TROPIC01's datasheet before diving into this example!
 * @par
 */

/**
 * @brief Session with H0 pairing keys
 *
 * @param h           Device's handle
 * @return            0 if success, otherwise -1
 */
static int session_H0(void)
{
    lt_handle_t h = {0};

    lt_init(&h);

    LT_LOG("%s", "Establish session with H0");

    LT_ASSERT(LT_OK, verify_chip_and_start_secure_session(&h, sh0priv, sh0pub, pkey_index_0));

    uint8_t in[100] = {0};
    uint8_t out[100] = {0};
    memcpy(out, "This is Hello World message from TROPIC01!!", 43);
    LT_LOG("%s", "lt_ping() ");
    LT_ASSERT(LT_OK, lt_ping(&h, out, in, 43));
    LT_LOG("\t\tMessage: %s", in);

    lt_deinit(&h);

    return 0;
}

int lt_ex_hello_world(void)
{
    LT_LOG("");
    LT_LOG("\t=======================================================================");
    LT_LOG("\t=====  TROPIC01 Hello World                                         ===");
    LT_LOG("\t=======================================================================");


    LT_LOG_LINE();
    LT_LOG("\t Session with H0 keys:");
    if(session_H0() == -1)  {
        LT_LOG("Error during session_H0()");
    }

    LT_LOG_LINE();

    LT_LOG("\t End of execution, no errors.");
    LT_FINISH_TEST();

    return 0;
}
