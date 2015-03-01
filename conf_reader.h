#include <stdbool.h>

enum _entry_types {_END, _STRING}; // Entry types

/* Entries in table of parameters and their data types. */
typedef struct {
    char *name;
    enum _entry_types  type;
} entry;

/* Entries in table of results. */
typedef union {
    char        *STRING;
} value;

/* Example use of the above.
 *
 * static entry plugin_param_config[] = {
 *     {"Full-screen-test", SC_STRING},
 * #define FULL_SCREEN_TEST plugin_param_values[0].STRING
 *     {NULL, _END}      // Keep this terminator.
 * };
 *
 * value plugin_param_values[ELEMENTS_IN_ARRAY(plugin_param_config)];
 */

int copyFile(char *src_file, char *dst_file);

bool get_config(char *file_name, entry *params, value *vals);
void set_config(char *file_name, entry *params, value *vals);

void release_config(entry *params, value *vals);

// functions to read and write a single key.
char *readStringKey(char *key, char *defaultValue, char *configFile);
bool readBoolKey(char *key, bool defaultValue, char *configFile);
void writeStringKey(char *section, char *key, char *keyValue, char *configFile);
void writeBoolKey(char *section, char *key, bool keyValue, char *configFile);
void removeKey(char *file_name, char *keyreq);

