/*
 * banker.h - FR5: Banker's safety algorithm.
 */
#ifndef BANKER_H
#define BANKER_H

/* argv[0] = state file. Returns EXIT_OK when the analysis ran (safe OR unsafe). */
int run_banker(int argc, char **argv);

#endif /* BANKER_H */
