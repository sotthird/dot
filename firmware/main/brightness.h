#pragma once

/* Cycle the display brightness with the power button. The choice is remembered across restarts.
 * Call after the screens exist; a board whose power button cannot be read simply never cycles. */
void brightness_start(void);
