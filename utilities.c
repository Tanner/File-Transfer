#include "utilities.h"

/**
 * Split a string into x variable.
 *
 * @param *input		String to split up
 * @param *delimiter	Delimiter to use
 * @param *size			Pointer that will contain the number of arguments found
 */
char ** split(char *input, char *delimiter, int *size) {
	char *input_copy = 0;
	char *temp = 0;

	// Count the number of chunks that exist in the string

	input_copy = calloc(strlen(input) + 1, sizeof(char));
	assert(input_copy);

	strncpy(input_copy, input, strlen(input));

	int count = -1;
	do {
		if (temp == 0) {
			temp = strtok(input_copy, delimiter);
		} else {
			temp = strtok(NULL, delimiter);
		}

		count++;
	} while (temp);

	// Create an array of that size (to hold all the chunks)

	char **split = calloc(count, sizeof(char *));
	assert(split);

	*size = count;

	// Begin to assign the chunks into the array
	strncpy(input_copy, input, strlen(input));

	temp = strtok(input_copy, delimiter);

	for (int i = 0; i < count; i++) {
		if (temp != NULL) {
			split[i] = malloc(strlen(temp) + 1);
			assert(split[i]);

			strcpy(split[i], temp);
		}

		temp = strtok(NULL, delimiter);
	}

	free(input_copy);

	return split;
}
