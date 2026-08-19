#include <stdio.h>
#include "handles.h"

int main(int argc, char *argv[]) {
    int existing_socket;
    int nonexisting_socket;

    setup_table();
    add_handle("test_handle", 6);
    existing_socket = lookup_handle("test_handle");
    nonexisting_socket = lookup_handle("nonexisting_handle");

    printf("Existing Socket: %d\n", existing_socket);
    printf("Non Existing Socket: %d\n", nonexisting_socket);

    add_handle("nonexisting_handle", 9);
    int test = lookup_handle("nonexisting_handle");
    printf("Now Existing Socket: %d\n", test);

    char *existing_handle;
    char *nonexisting_handle;
    existing_handle = lookup_socket(6);
    nonexisting_handle = lookup_socket(10);
    printf("Existing Handle: %s\n", existing_handle);
    printf("Non Existing Handle: %s\n", nonexisting_handle);
    existing_handle = lookup_socket(9);
    printf("Now Existing Handle: %s\n", existing_handle);

    return 0;
}