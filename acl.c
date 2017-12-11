#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

const int buf_size = 512; /*limit of chars per line*/
const char *pname; /*program name for errors*/

struct dirs{
	int count;
	char *dir[0];
};

// TODO: check all errno
#define IS_ERROR print_error(__LINE__)

int print_error(int linenum)
{
	int err = errno;
	if(0 != err){
		printf("%s: %s on line %d\n",pname,strerror(err),linenum);
		return 1;
	}
	return 0;
}

char *cut_str(char* s, char c)
{
	char *t = strchr(s,c);
	*t = '\0';
	return t;
}
int fseek_usr_entry(char *c_user, FILE *f)
{
	long f_pos = ftell(f);
	while(fgets(buf, buf_size, f) != NULL){
		// TODO: clean this logic
		usr = strtok(buf, "|");
		if(strcmp(usr, current_user) == 0){
			break;
		} else {
			usr = NULL;
			f_pos = ftell(f);
		}
	}
	/* go back one line */
	fseek(f, f_pos, SEEK_SET);

	return usr == NULL ? 0 : 1;
}

struct dirs *allowed_dir(FILE *f)
{
	char *current_user = getenv("USER");
	printf("  User: %s\n",current_user);
	char ch = 0;
	char *usr;
	/*
	 * supposed format:
	 * user|directory|directory|...|
	 *
	 * Note the final '|'
	 */
	if(!fseek_usr_entry(current_user, f)){
		/* errors */
	}

	char usr_entry[buf_size];
	fgets(usr_entry, buf_size, f);
	char *dir_list = strchr(usr_entry, '|') + 1;

	struct dirs *ret;
	if(IS_ERROR || !usr)
		goto allowed_dir_fail;

	int dir_num = 0;
	for(char *s = dir_list; s != NULL; s = strchr(s, '|') + 1)
		dir_num++;

	ret = malloc(sizeof(*ret) + sizeof(ret->dir) * dir_num);

	char *dir = strtok(dir_list, "|");
	do{
		int count = 0;

		cut_str(dir, '\n');
		ret->dir[count] = malloc(strlen(dir) + 1);
		strcpy(ret->dir[count], dir);
		ret->count = ++count;
	} while((dir = strtok(NULL, "|")) != NULL)

	return ret;/*free ret and friends later*/

allowed_dir_fail:
	ret = malloc(sizeof(*ret) + sizeof(ret->dir));
	ret->count = 0;
	ret->dir[0] = NULL;
	return ret;
}

void free_dirs(struct dirs *const p)
{
	for(int i = 0; i < count; ++i){
		free(p->dir[i]);
		p->dir[i] = NULL;
	}
	free(p);
}

/* Last item in cmd must be a (char*)NULL */
FILE *run_sh(const char *cmd, char *const args[])
{
	int filedes[2];

	pipe(filedes);
	pid_t pid = fork();

	if(!pid){
		close(filedes[0]);
		dup2(filedes[1],1);
		execvp(cmd,args);
		return NULL;
	} else if(pid == -1){
		close(filedes[0]);
		close(filedes[1]);
		//error
		return NULL;
	} else {
		close(filedes[1]);
		FILE *ret = fdopen(filedes[0],"r");
		wait(NULL);
		return ret;
	}
}

int dir_match(const struct dirs *const list, const char *const str)
{
	for(int i = 0; i < list->count; ++i){
		if(strcmp(list->dir[i],str) == 0)
			return 1;
	}
	return 0;
}
int is_dir_substr(const char *const str, const struct dirs *const list)
{
	int present = 0;
	for(int i = 0; i < list->count; ++i){
		if(strstr(str,list->dir[i]) == str){
			present = 1;
		}else{
			break;
		}
	}
	return present;
}

FILE *git_filenames_in_rev(const int test, char *const revision){
	char *cmd[7];
	if(test){
		cmd[0] = "echo";
		cmd[1] =" simple\nsrep\nsineapple";
		cmd[2] = (char*)NULL;
	} else {
		cmd[0] = "git";
		cmd[1] = "log";
		cmd[2] = "-1";
		cmd[3] = "--name-only";
		cmd[4] = "--pretty=format:'%s'";
		cmd[5] = revision;
		cmd[6] = (char*)NULL;
	}
	return run_sh(cmd[0], cmd);
}

int is_allowed(const struct dirs *const dir_list, char *rev_range)
{
	if(dir_match(dir_list,"."))/*user has rights in whole repo*/
		return 1;
	char *arg_rev[] = {"git", "rev-list", rev_range, (char*)NULL};
	FILE *rev_list = run_sh(arg_rev[0],arg_rev);

	char ch;

	char rev_buf[buf_size];
	int allowed = 1;

	while(fgets(rev_buf,buf_size,rev_list) != NULL){
		cut_str(rev_buf,'\n');

		FILE *file_list = git_filenames_in_rev(0, rev_buf);
		char file_buf[buf_size];

		while(fgets(file_buf,buf_size,file_list) != NULL){
			cut_str(file_buf,'\n');
			printf("  Check %s\n",file_buf);

			if(!is_dir_substr(file_buf,dir_list)){
				allowed = 0;
			}

			if(allowed)
				printf(".");
			else
				printf("x");
			printf(" %s\n", file_buf);
		}
		if(IS_ERROR)
			allowed = 0;
		printf("\n");
		fclose(file_list);
	}
	if(IS_ERROR)
		allowed = 0;
	fclose(rev_list);
	return allowed;
}

int main(int argc, char **argv)
{
	pname = argv[0];

	if(argc != 4){
		printf("Usage: %s REF OLDREV NEWREV\n",pname);
		IS_ERROR;
		return 1;
	}

	char *acl_file_name = "acl";

	FILE *f = fopen(acl_file_name, "r");

	if(!f){
		IS_ERROR;
		return 1;
	}

	struct dirs *user_dirs = allowed_dir(f);
	fclose(f);

	char rev_list[buf_size];

	// TODO: find out the max length of a revision.
	//TODO: check permission for creating or deleting a branch.
	const char empty[] = "0000000000000000000000000000000000000000";
	if((strcmp(argv[2],empty) == 0)||(strcmp(argv[3],empty) == 0)){
		printf("!!!Dangerously the branch!!!\n");
		return 0;
	} else {
	strcpy(rev_list,argv[2]);
	strcat(rev_list,"..");
	strcat(rev_list,argv[3]);
	}

	if(is_allowed(user_dirs,rev_list)){
		free_dirs(user_dirs);
		user_dirs = NULL;
		return 0;
	}
	printf("\n  Acl check Failed: files outside \"%s\"\n\n","Placeholder");
	printf("    If \"git rm\" FILE does not work try this:\n    https://help.github.com/articles/removing-sensitive-data-from-a-repository/\n");
	free_dirs(user_dirs);
	user_dirs = NULL;

	return 1;
}
