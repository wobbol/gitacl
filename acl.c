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
	char **dir;
}

// TODO: check all errno
#define PERRNO print_error(__LINE__);

void print_error(int linenum)
{
	int err = errno;
	//TODO: try not to use porcelain
	if(0 != strcmp(strerror(err),"Success"))
		printf("%s: %s on line %d\n",pname,strerror(err),linenum);
}


struct dirs *allowed_dir(FILE *f)
{
	char *current_user = getenv("USER");
	printf("  User: %s\n",current_user);
	char buf[buf_size];
	char ch = 0;
	char *tmp, *cur,*usr,*dir;
	/*
	 * supposed format:
	 * user|directory|directory|...|
	 *
	 * Note the final '|'
	 */
	while(fgets(buf,buf_size,f) != NULL){
		// TODO: clean this logic
		usr = strtok(buf,'|');
		if(strcmp(usr,current_user) == 0)
			break;
	}
	//TODO: check fgets error
	//TODO: if eof is reached user not found

	tmp = strchr(cur,'\n');
	if(!tmp)
		return NULL;
	*tmp = '\0';
	dir = malloc(tmp-cur+1);
	strcpy(dir,cur);
	return dir;/*free dir later*/
}

/* Last item in cmd must be a (char*)NULL */
FILE *run_sh(const char *cmd,char *const args[])
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

char *cut_str(char* s, char c)
{
	char *t = strchr(s,c);
	*t = '\0';
	return t;
}

int is_allowed(const char *dir, char *rev_range)
{

	if(!strcmp(dir,"."))
		return 1;
	char *arg_rev[] = {"git", "rev-list", rev_range, (char*)NULL};
	FILE *rev_list = run_sh(arg_rev[0],arg_rev);

	char file_buf[buf_size];
	char rev_buf[buf_size];
	char ch;

	int allowed = 1;
	while((ch = fgetc(rev_list)) != EOF){
		ungetc(ch,rev_list);

		fgets(rev_buf,buf_size,rev_list);
			cut_str(rev_buf,'\n');
	char *arg_file[] = {"git", "log", "-1", "--name-only", "--pretty=format:'%s'", rev_buf, (char*)NULL};
	/* char *arg_file[] = {"echo", "simple\nsrep\nsineapple", (char*)NULL}; */
	FILE *file_list = run_sh(arg_file[0],arg_file);
	fgets(file_buf,buf_size,file_list);
	cut_str(file_buf,'\n');
	printf("  Check %s\n",file_buf);

		while((ch = fgetc(file_list)) != EOF){
			ungetc(ch,file_list);

			fgets(file_buf,buf_size,file_list);
			cut_str(file_buf,'\n');
			if(strstr(file_buf,dir) != file_buf){
				allowed = 0;
				printf("x %s\n",file_buf);
			} else {
				printf(". %s\n",file_buf);
			}
		}
				printf("\n");

	}
	return allowed;
}

int main(int argc, char **argv)
{
	pname = argv[0];

	if(argc != 3){
		printf("Usage: %s REF OLDREV NEWREV",pname);
		PERRNO
		return 1;
	}

	char *acl_file_name = "acl";

	FILE *f = fopen(acl_file_name, "r");

	if(!f){
		PERRNO
		return 1;
	}

	char *dir = allowed_dir(f);
	fclose(f);
	// TODO: find out the max length of a revision.
	char rev_list[buf_size];
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

	if(is_allowed(dir,rev_list)){
		free(dir);
		return 0;
	}
	printf("\n  Acl check Failed: files outside \"%s\"\n\n",dir);
	printf("    If \"git rm\" FILE does not work try this:\n    https://help.github.com/articles/removing-sensitive-data-from-a-repository/\n");
	free(dir);

	return 1;
}
