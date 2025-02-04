#include "dberror.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// global error message pointer, used to store custom error description
char *RC_message;

/* print a message to standard out describing the error */
void printError (RC error) {
  	// check if customer error message exists
	if (RC_message != NULL)
		printf("EC (%i), \"%s\"\n", error, RC_message);
	else
		printf("EC (%i)\n", error);
}

char * errorMessage (RC error) {
	char *message;

	if (RC_message != NULL) {
        // allocate memory for the error message + custom message & formatting
		message = (char *) malloc(strlen(RC_message) + 30);
		sprintf(message, "EC (%i), \"%s\"\n", error, RC_message);
	}
	else {

        // allocate memory for the error message with only the error code
		message = (char *) malloc(30);
		sprintf(message, "EC (%i)\n", error);
	}
	return message;
}
