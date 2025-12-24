#ifndef SRC_INCLUDE_KERNEL_H
#define SRC_INCLUDE_KERNEL_H

/* Sends message to COM1 port and terminates.
   Warning, will need to change when working on paging. */
void kernel_panic (const char *msg, ...);


#endif /* SRC_INCLUDE_KERNEL_H */
