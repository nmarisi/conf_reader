# Purpose
A simple configuration file reader and writer (Key=Value)

This is a small C program to read and write configuration files.
Their structure is the following:

```
[Header]
Key=Value
```
Where the header is optional.

A valid file could therefore be:

```
workingDirectory=/home/myuser
isSavingAllowed=true
```
# Supported OSes

This project has been tested on OS X and Linux.

# Building and Use

In order to build the project as a standalone binary you can just type `make`.
There are a few tests than can be run by `make test`.

If yo wish to use the config_reader in your project just include the `conf_reader.h` header.

# Supported data types
At the moment, keys must be strings. 
In regards to values, there's support for booleans (`bool`) and strings.


# Functions

The conf_reader has the following functions:


```
void writeStringKey(char *section, char *key, char *keyValue, char *configFile)
```
This will write a key `key` with value `keyValue` of type string (`char *`) to the specified `configFile`.


```
void writeBoolKey(char *section, char *key, bool keyValue, char *configFile)
```
This will write a key `key` with value `keyValue`  of type `bool` to the specified `configFile`.


```
bool readBoolKey(char *key, bool defaultValue, char *configFile)
```
This will return a `bool` when reading `key` from the `configFile`. If the key is not found, the `defaultValue` will be returned instead.


```
char *readStringKey(char *key, char *defaultValue, char *configFile)
```
This will return a string(`char *`) when reading `key` from the `configFile`. If the key is not found, the `defaultValue` will be returned instead.

## Multiple keys at the same time

If neccesary, it is possible to read or write multiple keys at the same time. This is done by using an array of `struct entry` for keys and an array of `value` unions for the values.
The declaration for these types is provided below for your reference.

```
typedef struct {
    char *name;
    enum _entry_types  type;
} entry;

typedef union {
    char        *STRING;
} value;

```

The following functions are used to read and write keys in series:

```
void set_config(char *file_name, entry *params, value *vals)
bool get_config(char *file_name, entry *params, value *vals)
```

Where `file_name` is the path to the configuration file, `entry` is a point to the array of `struct entry` and `vals` is a pointer to an array of `struct value`. 

These functions will allocate the memory required to store the keys.
You can then use the following function to release the memory when you're done:

```
void release_config(entry *params, value *vals)
```


Here's an example for clarification:

```
#include <stdio.h>
#include "conf_reader.h"

static entry param_config[] = {
    {"UK", _STRING},
#define ENTRY_VALUE param_values[0].STRING
    {NULL, _END}      /* Keep this terminator. */
};

value param_values[1];


static entry keys_to_save[] = {
    {"UK", _STRING},
    {"France", _STRING},
    {"Denmark", _STRING},
    {NULL, _END}
};

value values_to_save[] = {"London", "Paris", "Copenhagen"};

int main(int argc, const char * argv[]) {
    int s;
    
    
    /* write test */
    set_config("/tmp/test", keys_to_save, values_to_save);
    
    /* read test */
    s = get_config("/tmp/test", param_config, param_values);
    if (s)
        printf("Final string: %s\n", ENTRY_VALUE);
    release_config(param_config, param_values);
    
    return !s;
}
```



