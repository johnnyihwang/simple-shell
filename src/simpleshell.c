#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

void myPrint(char *msg)
{
	write(STDOUT_FILENO, msg, strlen(msg));
}

int main(int argc, char *argv[]) 
{
	int mode = 1;
	FILE *fp = stdin;
	char errmsg[30] = "An error has occurred\n";
	char cmd_buff[514];
	char *pinput;
	char buf_tok[514];
	char *tok;
	char **p_args;
	int pcount;
	int i, j;
	int redirc;
	char *redir_p;
	int argc_child = 0;
	char **argv_child;
	char *hd;
	char wd[1024];
	pid_t pid;
	int fd;
	int fd2;
	char tmp_name[514];
	int ret_status;

	if (argc == 2)
	{
		// two arguments
		fp = fopen(argv[1], "r");
		if (!fp)
		{
			myPrint(errmsg);
			exit(1);
		}
		mode = feof(fp) ? 0 : 1;
	}

	else if (argc > 2)
	{
		myPrint(errmsg);
		exit(1);
	}

	while (mode)
	{
		cmd_buff[512] = '\n';
		if (argc == 1)
			myPrint("simpleshell> ");
		pinput = fgets(cmd_buff, 514, fp);
		if (!pinput)
			exit(0);
		// Handling cases where input is longer than 512 characters
		else if((cmd_buff[512] != '\n') && (cmd_buff[512] != '\0'))
		{
			myPrint(cmd_buff);
			cmd_buff[512] = '\n';
			do
			{
				pinput = fgets(cmd_buff, 514, fp);
				if (!pinput)
					exit(0);
				myPrint(cmd_buff);
			} while ((cmd_buff[512] != '\n') && (cmd_buff[512] != '\0'));
			myPrint(errmsg); // more than 512 characters
			continue;
		}

		// empty line check
		if (argc == 2)
		{
			strcpy(buf_tok, cmd_buff);
			tok = strtok(buf_tok, "\n\r\t ");
			while(tok)
			{
				if (*tok != '\0')
					break;
			}
			if (tok)
				myPrint(cmd_buff);
			else
				continue;
		}




		/* 
		 * Handling multiple commands
		 * First copy to buf_tok to count how many programs are called
		 * Then modify cmd_buff, separating into arguments for different programs
		 * p_args array then hold pointers to each arguments
		 */
		strcpy(buf_tok, cmd_buff);
		pcount = 0;
		tok = strtok(buf_tok, "\n\r;");
		// Counting how many programs are given to be run
		while(tok)
		{
			pcount++;
			tok = strtok(NULL, "\n\r;");
		}
		p_args = (char **) malloc(sizeof(char *) * pcount);
		// cmd_buff is separated by each program arguments
		tok = strtok(cmd_buff, "\n\r;");
		for(j = 0; tok; j++, tok = strtok(NULL, "\n\r;"))
		{
			p_args[j] = tok;
		}
		tok = NULL;

		// loop for each part of cmd_buff (for each program)
		for(j = 0; j < pcount; j++)
		{
			argc_child = 0;
			redirc = 0;
			redir_p = NULL;
			// file redirection check
			for(i = 0; (p_args[j][i] != '\0') && (redirc >= 0); i++)
			{
				if (p_args[j][i] == '>')
				{
					if (redirc >= 1)
					{
						redirc = -1;
						break;
					}
					else if (p_args[j][i+1] == '+')
						redirc = 2;
					else
						redirc = 1;
					redir_p = (p_args[j] + i);
				}
			}
			if (redirc < 0)
			{
				myPrint(errmsg);
				continue;
			}
			else if (redirc > 0)
			{
				*redir_p = '\0';
				redir_p += redirc;
				redir_p = strtok(redir_p, " \n\r\t");
				for(i = 0; (i < 1) && redir_p;)
				{
					if(*redir_p != '\0')
						i++;
					else
						redir_p = strtok(NULL, " \n\r\t");
				}
				tok = strtok(NULL, " \n\r\t");
				while (tok)
				{
					if ((*tok) != '\0')
						break;
					tok = strtok(NULL, " \n\r\t");
				}
				if ((!redir_p) || tok)
				{
					myPrint(errmsg);
					continue;
				}
			}
			tok = NULL;

			// copy cmd_buff for counting arguments using strtok
			strcpy(buf_tok, p_args[j]);
			// count number of arguments
			tok = strtok(buf_tok, " \t\n\r");
			while(tok)
			{
				// Handling multiple whitespaces
				if (*tok != '\0')
					argc_child++;
				tok = strtok(NULL, " \t\n\r");
			}

			// creating argv for child process
			argv_child = (char**) malloc(sizeof(char*) * (argc_child + 1));
			argv_child[0] = NULL;
			tok = strtok(p_args[j], " \t\n\r");
			for(i = 0; tok; tok = strtok(NULL, " \t\n\r"))
			{
				if (*tok != '\0')
				{
					argv_child[i] = tok;
					i++;
				}
			}
			argv_child[i] = NULL;
			if (argv_child[0] == NULL)
				continue;

			// checks for built-in commands
			if(!(strcmp(argv_child[0], "exit")))
			{
				if ((argc_child == 1) && (redirc == 0))
					exit(0);
				else
				{
					myPrint(errmsg);
					argc_child = 0;
					free(argv_child);
					continue;
				}
			}
			else if ((!(strcmp(argv_child[0], "cd"))))
			{
				if ((argc_child == 1) && (redirc == 0))
				{
					hd = getenv("HOME");
					if(chdir(hd))
						myPrint(errmsg);
				}
				else if ((argc_child == 2) && (redirc == 0))
				{
					if(chdir(argv_child[1]))
						myPrint(errmsg);
				}
				else
					myPrint(errmsg);
				argc_child = 0;
				free(argv_child);
				continue;
			}
			else if (!(strcmp(argv_child[0], "pwd")))
			{
				if ((argc_child == 1) && (redirc == 0))
				{
					getcwd(wd, 1024);
					myPrint(wd);
					myPrint("\n");
				}
				else
					myPrint(errmsg);
				argc_child = 0;
				free(argv_child);
				continue;
			}

			// redirection
			if (redirc == 1)
			{
				fd = open(redir_p, (O_CREAT | O_EXCL | O_WRONLY), (S_IRUSR | S_IWUSR));
				if (fd < 0)
				{
					myPrint(errmsg);
					continue;
				}
			}
			// advanced redirection
			else if (redirc == 2)
			{
				fd = open(redir_p, (O_CREAT | O_EXCL | O_WRONLY), (S_IRUSR | S_IWUSR));
				// file already exists
				if (fd < 0)
				{
					fd2 = -1;
					for (i = 0; fd2 < 0; i++)
					{
						sprintf(tmp_name, "%s%d", "tmp.tmp", i);
						fd2 = open(tmp_name, (O_CREAT | O_EXCL | O_WRONLY), (S_IRUSR | S_IWUSR));
					}
					if (close(fd2) < 0)
						myPrint("fd2-tmp file- close error?\n");
					unlink(tmp_name);
					rename(redir_p, tmp_name);
					fd = open(redir_p, (O_CREAT | O_EXCL | O_WRONLY), (S_IRUSR | S_IWUSR));
					if (fd < 0)
					{
						myPrint(errmsg);
						continue;
					}
				}
				else
					redirc = 1;
			}

			// forking
			pid = fork();
			if (pid == 0)
			{
				if (redirc > 0)
				{
					if ((dup2(fd, STDOUT_FILENO)) < 0)
					{
						myPrint("dup failed");
						exit(1);
					}
				}
				execvp(argv_child[0], argv_child);
				myPrint(errmsg);
				free(argv_child);
				exit(1);
			}
			else if (pid < 0)
			{
				myPrint("fork failed?\n");
			}
			else
			{
				wait(&ret_status);
				/*
				   if (ret_status)
				   myPrint(errmsg);
				 */
				argc_child = 0;
				free(argv_child);
				if (redirc == 1)
					close(fd);
				else if (redirc == 2)
				{
					close(fd);
					// redir_p output of child, tmp_name is the original file
					i = 0;
					fd = open(redir_p, (O_APPEND | O_WRONLY), (S_IRUSR | S_IWUSR));
					fd2 = open(tmp_name, O_RDONLY);
					i = read(fd2, buf_tok, 512);
					if ((fd < 0) || (fd2 < 0))
					{
						myPrint("moving back error?\n");
						continue;
					}
					while(i && (i >= 0))
					{
						buf_tok[i] = '\0';
						write(fd, buf_tok, i);
						i = read(fd2, buf_tok, 512);
					}
					if (i < 0)
					{
						myPrint(strerror(errno));
					}

					close(fd);
					unlink(tmp_name);
					close(fd2);

				}
			}
		}
		free(p_args);
		p_args = NULL;

		if (argc == 2)
			mode = feof(fp) ? 0 : 1;
	}
	return 0;
}
