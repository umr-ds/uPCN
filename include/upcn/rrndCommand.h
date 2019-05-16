#ifndef RRNDCOMMAND_H_INCLUDED
#define RRNDCOMMAND_H_INCLUDED

#include "parser.h"
#include "upcn.h"

enum rrnd_command_type {
	RRND_COMMAND_NONE,
	// Initialize the SGP4 algorithm for inferring 3D satellite positions
	RRND_COMMAND_INITIALIZE_SATPOS,
	// Request that a contact inference is performed (result stored in GS)
	RRND_COMMAND_INFER_CONTACT,
	// Request a JSON string with all current data for the specific GS
	RRND_COMMAND_QUERY_GS,
	// Update GS information with the provided metrics (prob., cap.)
	RRND_COMMAND_INTEGRATE_METRICS,
	// For bounds check in parser
	_RRND_COMMAND_LAST
};

struct rrnd_command {
	enum rrnd_command_type type;
	// The (target) GS EID; for INFER_CONTACT, QUERY_GS, INTEGRATE_METRICS
	char *gs;
	// The source GS EID (which "reports" the data); for INTEGRATE_METRICS
	char *source_gs;
	// The TLE string, separated by a single '\n'; for INITIALIZE_SATPOS
	char *tle;
	// Probability metric for INTEGRATE_METRICS
	float gs_reliability;
};

// The functions are not executed in a separate task currently
void rrnd_execute_command(struct rrnd_command *cmd);

#endif /* RRNDCOMMAND_H_INCLUDED */
