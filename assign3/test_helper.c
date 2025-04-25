
#include "test_helper.h"

// Global variable for test name
char *testName;

// Minimal error message function for demonstration
char *errorMessage(int rc) {
	// You can expand this as needed for your RC codes
	char *msg = (char *)malloc(64);
	snprintf(msg, 64, "Error code %d", rc);
	return msg;
}
