#pragma once

/* USB serial link to the host: newline-terminated text lines, plus binary image
 * payloads announced by an "IMG:<w>x<h>:<bytes>:<rrggbb>" header line. */

/* Start the task that reads the link and feeds the app registry. */
void serial_link_start(void);

void serial_link_send(const char* msg);
