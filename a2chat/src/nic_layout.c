/* Compiled and discarded. Fails the build if struct nic is not the
 * overlay header the assembler writes. */
#include "nic.h"
#include <stddef.h>

unsigned nic_off_flags(void) { return (unsigned)offsetof(struct nic, flags); }
unsigned nic_off_stream(void) { return (unsigned)offsetof(struct nic, stream_default); }
unsigned nic_off_size(void) { return (unsigned)offsetof(struct nic, load_size); }
unsigned nic_off_probe(void) { return (unsigned)offsetof(struct nic, probe); }
unsigned nic_off_wall(void) { return (unsigned)offsetof(struct nic, wall); }
unsigned nic_off_name(void) { return (unsigned)offsetof(struct nic, clock_name); }
unsigned nic_off_end(void) { return (unsigned)sizeof(struct nic); }
