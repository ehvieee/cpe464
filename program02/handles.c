#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "handles.h"

static int currentTableSize = 0;

// Table Linked List
struct Table {
    int socket;
    char *handle;
    struct Table *next;
};

static struct Table *startPointer;
static struct Table *iterPointer;

void setup_table() {
    // initialize pointers
    startPointer = NULL;
    iterPointer = startPointer;
}

void add_handle(char *handle_name, int socket) {
    // create a new handle
    struct Table *newHandle;
    newHandle = malloc(sizeof(struct Table));

    // set up new handle
    newHandle->socket = socket;
    newHandle->handle = strdup(handle_name);
    newHandle->next = NULL;

    // keep track of starting handle
    if (currentTableSize == 0) {
        // assign start to the new handle
        startPointer = newHandle;
        iterPointer = startPointer;
    }
    else {
        struct Table *tempPointer;
        tempPointer = startPointer;
        
        // go to end of linked list
        while (tempPointer->next != NULL) {
            tempPointer = tempPointer->next;
        }

        tempPointer->next = newHandle;
    }

    // increment table size
    currentTableSize++;
}

// returns socket if handle name found and valid, otherwise returns -1
int lookup_handle(char *handle_name) {
    struct Table *searchPointer;
    searchPointer = startPointer;

    // iterate through Table
    while (searchPointer != NULL) {
        // found handle in table
        if (strcmp(searchPointer->handle, handle_name) == 0) {
            return searchPointer->socket;
        }

        // go to next handle entry
        searchPointer = (searchPointer->next);
    }

    return -1;
}

// returns handle name if socket found and valid, otherwise returns NULL
char *lookup_socket(int socket) {
    struct Table *searchPointer;
    searchPointer = startPointer;

    // iterate through Table
    while (searchPointer != NULL) {
        // found socket in table
        if ((searchPointer->socket) == socket) {
            // check if it has been removed
            if (searchPointer->handle != NULL) {
                return searchPointer->handle;
            }
            else {
                return NULL;
            }
        }

        // go to next handle entry
        searchPointer = (searchPointer->next);
    }

    return NULL;
}

void remove_handle(int socket) {
    struct Table *searchPointer;
    searchPointer = startPointer;
    struct Table *prevPointer = NULL;

    // iterate through entire linked list
    while (searchPointer != NULL) {
        // element found
        if ((searchPointer->socket) == socket) {
            // case for if element is startPointer
            if (prevPointer == NULL){
                startPointer = searchPointer->next;
            }
            else {
                // remove searchPointer from link
                prevPointer->next = searchPointer->next;
            }

            // free element
            free(searchPointer);

            // decrement table count
            currentTableSize--;

            break;
        }

        // update pointers to next element
        prevPointer = searchPointer;
        searchPointer = searchPointer->next;
    }
}

int get_table_size() {
    return currentTableSize;
}

// returns handle name of next item if it exists, else reset pointer and returns NULL
char *get_handle() {
    // check if we're at the end of the linked list
    if (iterPointer != NULL) {
        // save current handle
        char *handle = iterPointer->handle;

        // move iterPointer to next handle
        iterPointer = iterPointer->next;

        // return handle name
        return handle;
    }

    // reset iterPointer to the start of the list
    iterPointer = startPointer;
    return NULL;
}