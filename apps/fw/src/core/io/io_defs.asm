// Copyright (c) 2016 Qualcomm Technologies International, Ltd.
//   %%version
/**
 * /file
 *
 * Select the right io_defs.asm based on the chip target
 */

/*lint --e{451} Double inclusion protection intentionally absent */

#ifdef CHIP_CRESCENDO
# ifdef CHIP_REVISION_DEV
#  include "io/crescendo/dev/io/io_defs.asm"
# endif /* CHIP_REVISION_DEV */
#endif /* CHIP_CRESCENDO */


#ifdef CHIP_AURA
# ifdef CHIP_REVISION_D00
#  include "io/aura/d00/io/io_defs.asm"
# endif /* CHIP_REVISION_D00 */
#endif /* CHIP_AURA */


#ifdef CHIP_PRESTO
# ifdef CHIP_REVISION_D00
#  include "io/presto/d00/io/io_defs.asm"
# endif /* CHIP_REVISION_D00 */
#endif /* CHIP_PRESTO */
