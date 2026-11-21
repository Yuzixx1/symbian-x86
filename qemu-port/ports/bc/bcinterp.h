/*
 * bcinterp.h -- small freestanding interpreter for a bc-compatible subset.
 *
 * Supports: arbitrary-precision decimal numbers, + - * / % ^, comparisons,
 * scale, variables a-z plus scale, assignment (+= -= *= /= %= ^=),
 * define/return functions (with recursion), if/else, while, for, print,
 * break/continue, and C-style and hash comments.
 *
 * The host supplies an emit callback that receives one complete marker line
 * per value (the host decides how to carry it over EUSER ordinal 915).
 */
#ifndef PORT_BC_BCINTERP_H
#define PORT_BC_BCINTERP_H

typedef void (*BcEmit)(const char* text);

void bc_setup(BcEmit aEmit);   /* install marker sink; call once */
void bc_run(const char* text); /* parse and execute one script/line */

#endif /* PORT_BC_BCINTERP_H */
