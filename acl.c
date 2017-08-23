#include <file.h>
#include <string.h>
const int line_limit = 10;
const int num_items_limit = 10;
const int line_length_limit = 200;
const int string_length_limit = 15;
struct acl{
	char name[num_items_limit][string_length_limit];
	char allowed_paths[num_items_limit][num_items_limit][string_length_limit];
int get_acl_access_data(FILE *file){
char access[line_limit][line_length_limit] = {0};
	while (!EOF||line_limit) split file at newline


