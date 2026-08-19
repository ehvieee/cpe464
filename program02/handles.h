
// 	Writen - HMS April 2017
//  Supports TCP and UDP - both client and server


#ifndef __HANDLES_H__
#define __HANDLES_H__

void setup_table();
void add_handle(char *handle_name, int socket);
int lookup_handle(char *handle_name);
char *lookup_socket(int socket);
void remove_handle(int socket);
int get_table_size(void);
char *get_handle(void);

#endif