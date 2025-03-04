#include "strfuncs.h"

int str_start(const char *haystack, const char *needle) { // Checks if haystack begins with needle
    return ! strncmp(haystack, needle, strlen(needle));
}

int str_end(const char *haystack, const char *needle) { // Checks if haystack ends with needle
    return ! strcmp(needle, haystack + strlen(haystack) - strlen(needle));
}