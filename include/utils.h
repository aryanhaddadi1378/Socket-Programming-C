/**
 * Prints the given message in the stderr and aborts the program.
 * @param msg The message to be written into stderr.
 */
void printError(char *msg);

/**
 * Print the given message in the stdout.
 * @param msg The message to be written into stdout
 */
void printMsg(char *msg);

/**
 * Sets all elements of the given char pointer to NULL.
 * NOTE: The length of the pointer must be MAX_DATA_SIZE.
 * @param ptr Pointer to set all its characters to NULL.
 */
void clear(char *ptr);