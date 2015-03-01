#define STAND_ALONE_TEST
#define _GNU_SOURCE 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#ifdef STAND_ALONE_TEST
typedef int bool;
#define true 1
#define false 0
#endif

#include "conf_reader.h"

#define BUFSIZE 4096

// Make a file copy with permissions u+rw. */
int file_copy(char *src_file, char *dest_file)
{
    int source_fd = open(src_file, O_RDONLY);
    
    if (source_fd != -1) {

        /* Create destination file with user rw permissions.*/
        int destFd = open(dest_file, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR);
        if ( destFd == -1) {
            return -1;
        }

        ssize_t ret_in;
        ssize_t ret_out;
        char buffer[BUFSIZE];

        while((ret_in = read (source_fd, &buffer, BUFSIZE)) > 0) {
            ret_out = write (destFd, &buffer, (ssize_t) ret_in);
            if(ret_out != ret_in){
                return -1;
           }
       }
    
        close(source_fd);
        close(destFd);
        
        return 0;
    }

    return -1;
}

/* Read a file containing lines of the form "key = value" and use
 * the first table to parse the results into the second.
 */

bool get_config(char *file_name, entry *params, value *vals)
{
    FILE        *fp;
    entry        *tep;
    char        *cp, *ep, *ov;
    bool      got_eq;
    int          n;
    char         buf[1024];

    fp = fopen(file_name, "r");
    if (!fp)
        return false;

    /* Get file lock. */
    if (flock(fileno(fp), LOCK_EX)) {
        perror("error in file locking");
        return false;
    }
    /* Flush fp just in case its internal buffer was filled before
     * obtaining the lock. */
    if (fflush(fp)) {
        perror("error in flushing file");
    }
 

    while (fgets(buf, sizeof buf, fp)) {
#ifdef STAND_ALONE_TEST
        printf("Read line %s\n", buf);
#endif

        /* Skip initial blanks and isolate a word.*/

        cp = buf;
        while (*cp == ' ' || *cp == '\t')
            ++cp;
        if (*cp == '\0' || *cp == '\n' || *cp == ';' ||*cp == '#')
            continue;           /* Blank or comment - ignore. */
        ep = cp + 1;
        while (*ep && *ep != '=' && *ep != ' ' && *ep != '\t'  &&
               *ep != '\n')
            ++ep;
        if (!*ep || *ep == '\n') /* Just a word */
            continue;
        got_eq = (*ep == '=');

        /* Check for a known parameter name. */

        *ep = '\0';
        for (tep = params; tep->type != _END; ++tep) {
            if (!strcasecmp(cp, tep->name))
                break;
        }

        if (tep->type == _END)
            continue;           /* Unknown parameter - ignore. */

        /* Clear white-space. */

        cp = ep + 1;
        while (*cp == ' ' || *cp == '\t')
            ++cp;

        if (!got_eq) {
            if (*cp != '=')
                continue;       /* No equals sign - ignore. */
            do
                ++cp;
            while (*cp == ' ' || *cp == '\t');
        }
        if (*cp == '\n' || !*cp)
            continue;           /* No RHS. */


        /* Concatentate lines ending with a backslash. */

        ep = cp;
        for(;;) {
            ep = ep + strlen(ep) - 1;
            if (*ep != '\n')
                break;
            *ep-- = '\0';
            if (*ep != '\\')
                break;
            if (ep >= buf + (sizeof buf) - 2 ||
                !fgets(ep, buf + (sizeof buf) - ep, fp)) {
                *ep-- = '\0';
                break;
            }
        } 

        /* Strip trailing white-space. */

        while (ep > cp && (*ep == ' ' || *ep == '\t'))
            *ep-- = '\0';

        /* Do something with the string. */

        switch (tep->type) {
        case _STRING:
            ov = vals[tep - params].STRING;
            if (ov)
                free(ov);    /* Discard previous value. */

            n = ep - cp + 2; /* +2 in order to include null terminator. */
            ep = malloc(n);   /* Allocate string space. */
            if (ep)
                strcpy(ep, cp);
            vals[tep - params].STRING = ep;
#ifdef STAND_ALONE_TEST
            printf("Storing %d bytes:%s\n", n, ep);
#endif
            break;
        default:
            break;
        }
    }
    /* Flush FILE buffer and release file lock.*/
    if (fflush(fp)) {
        perror("file flush error");
    }
    if (flock(fileno(fp), LOCK_UN)) {
        perror("flock error");
    }

    fclose(fp);
   
    return true;
}

/* helper function for saveconf, saves key-val pair into fp */

void putstr(char *key, char *val, FILE *fp)
{
   char *savestr = malloc(strlen(key) + strlen(val) + 3);
   if (!savestr) {
       fprintf(stderr, "%s\n", strerror(errno));
       exit(1); 
   }
   memset(savestr, 0, strlen(key) + strlen(val) + 3);
   strncpy(savestr, key, strlen(key));
   strncat(savestr, "=", 1);
   strncat(savestr, val, strlen(val));
   strncat(savestr, "\n", 1);
   
   if ( !fputs(savestr, fp) ) {
       fprintf(stderr, "%s\n", strerror(errno));
   }

   free(savestr);
}

/* Function used to save a key=value pair into the
 * specified configuration file.
 */

void
saveconf(char *file_name, char *keyreq, char *keyval, char *sectionHeader) 
{
    int fd; /* used to generate a random file */
    FILE *tmpfp;
    FILE *fp;
    
    char buf[1024];
    char *keyptr, *valptr, *tmpptr, *headerptr; /* character pointers */
    char tmpfile[15];

    bool found = 0; // true if key has been found before EOF, replace with bool
    bool foundHeader = 0; /* true if header has been found */

    /* create an open tmp file */ 
    strncpy(tmpfile, "/tmp/sc.XXXXXX", sizeof tmpfile);
    
    if ((fd = mkstemp(tmpfile)) == -1 ||
       (tmpfp = fdopen(fd, "w+")) == NULL) {
          if (fd != -1) {
            unlink((const char *)tmpfp);
            close(fd);
          }
          fprintf(stderr, "%s: %s\n", tmpfile, strerror(errno));
          return;
    }

    
    fp = fopen(file_name, "r+");
    if (!fp) {
        fprintf(stderr, "error in file opening\n");
        return;
    }

    /* Get file lock. */
    if (flock(fileno(fp), LOCK_EX)) {
        perror("error in file locking");
        return;
    }

    /* Flush fp just in case its internal buffer was filled before
     * obtaining the lock. */
    if (fflush(fp)) {
        fprintf(stderr, "error in flushing file\n");
    }
    
    while (fgets(buf, sizeof buf, fp)) {
   
        
        keyptr = buf;
        
        /* get rid of initial blanks and stop at delimiter */
        while (*keyptr == ' ' || *keyptr == '\t') {
            ++keyptr;
        }
        
        /* save comments and section headers */
        if (*keyptr == ';') { 
            fputs(keyptr, tmpfp);
            continue;
        }
        
        /* Find header */
        if ( *keyptr == '[') {
            
            if (!found && foundHeader) { /*save new key in appropiate header */
                found = true;
                putstr(keyreq, keyval, tmpfp);
                fputs(keyptr, tmpfp); /* save next header in tmp file. */
                continue;
            }
            fputs(keyptr, tmpfp); /* save header in tmp file. */
            
            headerptr = keyptr; 
            while (*headerptr != ']') {
                ++headerptr;
            }
            *headerptr = '\0';
            keyptr++; /* move one beyond [ */ 
            if (!strncmp(keyptr, sectionHeader, strlen(keyptr)+1)) {
                foundHeader = true;
            }
            continue;
        }

        /* ignore newlines and  empty lines */
        if (*keyptr == '\n' || *keyptr == '\0') {
            continue;
        }

        valptr = keyptr; /* valptr will be used to store the value of this key */
        
        /*convert key into a proper string by adding a null char at the end */
        while (*valptr && *valptr != '=' && *valptr != ' ' && *valptr != '\t' &&
               *valptr != '\n' ) {
            ++valptr;
        }

        if (!*valptr || *valptr == '\n') {  //no value for this key, ignore
            continue; 
        } else {
            *valptr = '\0';
            valptr++; // move cursor away from end of string symbol
        }

        //remove spaces between key and = and between = and value
        while (*valptr == ' ' || *valptr == '=') {
            ++valptr;
        }
        
        tmpptr = valptr;

        /* Concatenate lines ending in backlash */
        for(;;) {
            while (*tmpptr != '\n' && *tmpptr != '\\') {
                tmpptr++;
            }
            if (*tmpptr == '\\') { /* concatenate */ 
                fgets(tmpptr, buf + (sizeof buf) - tmpptr, fp);
            } else { 
                *tmpptr = '\0';
                break;
            }
        }
        
        /*find end of value */
        while (*tmpptr && *tmpptr != '=' && *tmpptr != ' ' && *tmpptr != '\t' &&
               *tmpptr != '\n' ) {
            ++tmpptr;
        }

         *tmpptr='\0';

        /* if key already exists, store new value in tmp file */
        
        if(!strncmp(keyptr, keyreq, strlen(keyptr)+1) ) {
            putstr(keyreq, keyval, tmpfp);
            found = 1;


        } else {
            /* save existing keys into tmp file */
            putstr(keyptr, valptr, tmpfp);
        }
   }

   /* reached EOF without finding key or header, save both of them at the end */
   if (!found) {

      if (!foundHeader) {
      	/* Save header +1 for null char and +3 for header brackets and newline. */
          if (sectionHeader) {
              char *headerStr = malloc(strlen(sectionHeader) + 4);
              sprintf(headerStr, "[%s]\n", sectionHeader);
              fputs(headerStr, tmpfp);
              free(headerStr);
          }
      	/* Save key. */
      	putstr(keyreq, keyval, tmpfp);
    
      } else {/* foundHeader is true */    
    	/* Reached EOF without finding key but the last header was foundHeader
        so adding an entry to the new key. */
      	putstr(keyreq, keyval, tmpfp);
      }
   }  
    

   fputs("\n", tmpfp);
   
   /* Flush FILE buffer and release file lock.*/
   if (fflush(fp)) {
        perror("error in flushing file");
    }
   
   if (flock(fileno(fp), LOCK_UN)) {
       perror("can't release filelock");
   }

   fclose(fp);
   fclose(tmpfp);
   if (file_copy(tmpfile, file_name))
       fprintf(stderr, "Unable to copy %s into %s\n", tmpfile, file_name); 
   //remove(tmpfile);
       

}


/* Save a value into a file in the form of "key = value",
 * counterpart to get_config()
 */

void set_config(char *file_name, entry *params, value *vals)
{
     for (; params->type != _END; params++, vals++) {
         saveconf(file_name, params->name, vals->STRING, NULL);
     }
}

void release_config(entry *params, value *vals)
{
    while (params->type != _END) {
        if (params->type == _STRING && vals->STRING) {
            free(vals->STRING);
            vals->STRING = NULL;
        }
        ++params;
        ++vals;
    }
}

/* This function removes a key-value pair from a config file.
 * It's original purpose is to remove key-value pairs from wfclient.ini
 * NB: This function only works correctly if the key-value pair only
 *     occupies one line! A future enhancement should take into account
 *     values that span an indefinate number lines. 
 */
void removeKey(char *file_name, char *keyreq) 
{
    int fd; /* used to generate a random file */
    FILE *tmpfp;
    FILE *fp;
    
    char buf[1024];
    char *bufCopy;
    char *keyptr, *tmpptr; /* character pointers */
    char tmpfile[15];


    /* create an open tmp file */ 
    strncpy(tmpfile, "/tmp/sc.XXXXXX", sizeof tmpfile);
    if ((fd = mkstemp(tmpfile)) == -1 ||
       (tmpfp = fdopen(fd, "w+")) == NULL) {
          if (fd != -1) {
            unlink((const char *)tmpfp);
            close(fd);
          }
          fprintf(stderr, "%s: %s\n", tmpfile, strerror(errno));
          return;
}
    
    /* open the file, create it if it doesn't exist */
    fp = fopen(file_name, "a+");
    if (!fp) {
        fprintf(stderr, "error in file opening\n");
        return;
    }

    /* Get file lock. */
    if (flock(fileno(fp), LOCK_EX)) {
        perror("error in file locking");
        return;
    }

    /* Flush fp just in case its internal buffer was filled before
     * obtaining the lock. */
    if (fflush(fp)) {
        fprintf(stderr, "error in flushing file\n");
    }

    
    while (fgets(buf, sizeof buf, fp)) {
   
        bufCopy = strdup(buf); 
        keyptr = buf;
        
        /* get rid of initial blanks and stop at delimiter */
        while (*keyptr == ' ' || *keyptr == '\t') {
            ++keyptr;
        }
        tmpptr = keyptr;

        /* Find end of key. */
        while (*tmpptr && *tmpptr != '=' && *tmpptr != ' ' && *tmpptr != '\t')
            ++tmpptr;

        *tmpptr = '\0';
        
        if (strcmp(keyptr, keyreq)) {
            fputs(bufCopy, tmpfp);
        } else { /* If we find the key, don't copy the line to the tmp file. */
            continue;
        }

        //ENHANCEMENT: what about multiple line keys? need to join values together

    }

    /* Flush FILE buffer and release file lock.*/
    if (fflush(fp)) {
         perror("error in flushing file");
     }
   
    if (flock(fileno(fp), LOCK_UN)) {
        perror("can't release filelock");
    }

    fclose(fp);
    fclose(tmpfp);
    rename(tmpfile, file_name);
}

/* Convenience function to read a single Key. */
char *readStringKey(char *key, char *defaultValue, char *configFile)
{
    entry keyToRead[2];
    keyToRead[0].name = key;
    keyToRead[0].type = _STRING;
    keyToRead[1].name = NULL;
    keyToRead[1].type = _END;

    value keyValue[1] = {{0}};

    bool s = get_config(configFile, keyToRead, keyValue);

    if (s) { /* If get_config was succesful. */
        if (keyValue[0].STRING)    /* If key was found. */
            return keyValue[0].STRING;
    }

    
    /* If get_config failed or key was not found. */
    if (defaultValue)  /* only copy default val if not NULL. */
            return strdup(defaultValue); /* Return an alloc'd string. */
        else
            return NULL;
 
}

bool readBoolKey(char *key, bool defaultValue, char *configFile)
{
    int i;
    
    char *keyVal = readStringKey(key,"False", configFile);
    /* Convert key value to all lowercase for comparison. */
    
    if (!keyVal) 
        return false;
    
    for (i = 0; keyVal[i]; i++) {
        keyVal[i] = tolower(keyVal[i]);
    }

    if (!strcmp(keyVal, "true") || !strcmp(keyVal, "on") || 
            !strcmp(keyVal, "yes")) {
        return true;
    } else if (!strcmp(keyVal, "false") || !strcmp(keyVal, "off") ||
        !strcmp(keyVal, "no")) {
        return false;
    } else {
        return defaultValue;
    }
}

void writeStringKey(char *section, char *key, char *keyValue, char *configFile)
{
    saveconf(configFile, key, keyValue, section);
}

void writeBoolKey(char *section, char *key, bool keyValue, char *configFile)
{
    if (keyValue) {
        writeStringKey(section, key, "True", configFile);
    } else {
        writeStringKey(section, key, "False", configFile);
    }
}
/* Main() function for stand-alone testing. */

#ifdef STAND_ALONE_TEST
/* Define parameters and table. */

static entry param_config[] = {
    {"UK", _STRING},
#define FISH_VALUE param_values[0].STRING
    {NULL, _END}      /* Keep this terminator. */
};

value param_values[1];


static entry keys_to_save[] = {
    {"UK", _STRING},
    {"Denmark", _STRING},
    {"France", _STRING},
    {NULL, _END}
};

value values_to_save[] = {"London", "Paris", "Copenhagen"};

int main(int argc, char **argv)
{
    bool     s;
    
    /* write test */
    FILE *fp = fopen("/tmp/test", "a+");
    fclose(fp);
    
    set_config("/tmp/test", keys_to_save, values_to_save);
    
    /* read test */
    s = get_config("/tmp/test", param_config, param_values);
    if (s)
        printf("Final string: %s\n", FISH_VALUE);
    release_config(param_config, param_values);
    
    return !s;
}

#endif


