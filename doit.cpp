#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
using namespace std;
/*
 * doit.c
 *
 *  Created on: Aug 29, 2016
 *      Author: matt
 *
 *
 *
 *      Part 3 - comparisons between my shell and Linux
 *      The main difference between the shells is how they react when a background
 *      process finishes. As described in the homework assignment, ours immediately alerts the
 *      user that it has finished, and prints out the statistics. This causes a small bug where
 *      the info will print over the user prompt, and the next time the user enters a command
 *      the "==>" will not be there. This glitch is not present in the normal shell.
 *      With the exception of a few other features (such as using the arrow keys to view previous commands)
 *      the majority of the functionality is identical. The most noticeable difference is the lack of the
 *      ability to "pipe" information from one command to another with the |. This could be recreated by using
 *      the pipe() command that I have used here to send PIDs from the grandchildren to the parent.
 */



int taskpids[10];
int middlepids[10];
string tasknames[10];
int io[2];

// convenience method for calculating time elapsed
int mselapsed(timeval start, timeval end)
{
	return (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
}

// part 1 of the project, runs and provides data
// second arg serves as a flag and a counter for background processes
// 0 = running normally, 1-10 = backgrounding
void runit(char* argv[], int bkgnum)
{
	struct rusage usage;
	struct timeval starts[3];
	struct timeval ends[3];

	getrusage(RUSAGE_CHILDREN, &usage);
	starts[0] = usage.ru_utime; // user cpu time
	starts[1] = usage.ru_stime; // system cpu time
	gettimeofday(&starts[2], NULL); // clock time

	int pid = fork();
	if(pid == -1) // error
	{
		exit(1);
	}
	else if(pid == 0) // child
	{
		execvp(argv[1],&argv[1]);
	}
	else // parent
	{
		if(bkgnum > 0) write(io[1], &pid, sizeof(pid));
		waitpid(pid, NULL, 0); // wait for the child process to die
		if(bkgnum > 0) cout << "[" << bkgnum << "] " << pid << " Completed " << argv[1] << endl;
		getrusage(RUSAGE_CHILDREN, &usage);
		ends[0] = usage.ru_utime;
		ends[1] = usage.ru_stime;
		long ivcs = usage.ru_nivcsw; // involuntary context switch
		long vcs = usage.ru_nvcsw; // voluntary context switch
		long major = usage.ru_majflt; // major page faults
		long minor = usage.ru_minflt; // minor page faults
		long mrss = usage.ru_maxrss; // max resident set size used
		gettimeofday(&ends[2], NULL);
		cout << "System resources used by " << argv[1] << ":" << endl;
		cout << "CPU time used-user: " << mselapsed(starts[0], ends[0]) << "ms" << endl;
		cout << "CPU time used-system: " << mselapsed(starts[1], ends[1]) << "ms" << endl;
		cout << "Wall clock time elapsed: " << mselapsed(starts[2], ends[2]) << "ms" << endl;
		cout << "Involuntary context switches: " << ivcs << endl;
		cout << "Voluntary context switches: " << vcs << endl;
		cout << "Major page faults: " << major << endl;
		cout << "Minor page faults: " << minor << endl;
		cout << "Maximum resident set size used: " << mrss << "kb" << endl;
		if(bkgnum > 0) exit(0);
	}
}

void runshell()
{
	string input = "lol";
	char* inarr;
	const char* filter = (char*)" ";
	char** args = (char**)calloc(33, sizeof(char*));
	for(int xd = 0; xd<33; xd++) args[xd] = (char*)calloc(128, sizeof(char));
	strcpy(args[0], "./doit");
	char* term;
	bool backgrnd = false;
	int bkgcnt = 1;
	int status;
	do { // main loop; each cycle through represents one full line of commands being handled

		// reset variables
		backgrnd = false;

		// record user input; separate into terms
		if(input != "") cout << "==>";
		getline(cin, input);
		inarr = strndup(input.c_str(), 128);
		if(strcmp(inarr, "") == 0) continue;
		term = strtok(inarr, filter);
		strcpy(args[1], term);
		for(int i = 2; i < 32; i++)
		{
			term = strtok(NULL, filter);
			if(term == NULL)
			{
				if(strcmp(args[i-1], "&") == 0)
				{
					// enables backgrounding; removes & so it isn't passed as an arg
					backgrnd = true;
					i--;
				}
				args[i] = '\0';
				break;
			}
			else
			{
				// line is important for when using a command with more args than the previous one
				// ex, running "firefox" sets args[2] to null, but "ls -l" requires args[2] to still be a char.
				if(args[i] == NULL) args[i] = (char*)malloc(sizeof(char));
				strcpy(args[i], term);
			}
		}


		// check if any of the background tasks from before have been killed
		for(int i = 0; i < 10; i++)
		{
			if(taskpids[i] != 0)
			{
				status = waitpid(middlepids[i], NULL, WNOHANG); //check status without actually waiting
				if(status != 0)
				{ // if dead, wipe old data from the arrays
					taskpids[i] = 0;
					middlepids[i] = 0;
					tasknames[i] = "";
				}
			}
		}

		// handle special commands
		if(strcmp(args[1], "exit") == 0)
		{
			//waits for all children to die first
			for(int i = 0; i < 10; i++)
			{
				if(taskpids[i] != 0)
				{
					cout << "Waiting for " << taskpids[i] << " " << tasknames[i] << " to end before quitting..." << endl;
					waitpid(middlepids[i], NULL, 0);
				}
			}
			exit(0);
		}
		else if(strcmp(args[1], "jobs") == 0)
		{
			for(int i = 0; i < 10; i++)
			{
				if(taskpids[i] != 0)
				{
					cout << "[" << i+1 << "] " << taskpids[i] << " " << tasknames[i] << endl;
				}
			}
		}
		else if(strcmp(args[1], "cd") == 0)
		{
			int ret = chdir(args[2]);
			if(ret == 0) cout << "current directory changed to " << get_current_dir_name() << endl;
			else cout << "Error changing directory to " << args[2] << ": " << strerror(errno) << endl;
		}
		else // handle normal commands
		{
			if(backgrnd)
			{
				//find the next count
				for(int i = 0; i < 11; i++)
				{
					if(i == 10) bkgcnt = -1;
					else if(taskpids[i] == 0)
					{
						bkgcnt = i;
						break;
					}
				}

				pipe(io);
				middlepids[bkgcnt] = fork();
				if(middlepids[bkgcnt] == -1) exit(-1);
				else if(middlepids[bkgcnt] == 0) // child
				{
					runit(args, bkgcnt+1); // runs command then terminates, as if i had called ./doit $args on cmd
				}
				else // parents
				{
					close(io[1]);
					read(io[0], &taskpids[bkgcnt], sizeof(int));
					tasknames[bkgcnt] = args[1];
					cout << "[" << bkgcnt+1 << "] " << taskpids[bkgcnt] << " Started " << tasknames[bkgcnt] << endl;
				}
			}
			else
			{
				runit(args, 0);
			}
		}
	} while (1 < 2);
}

int main(int argc, char* argv[])
{
	if(argc > 1) // if command was specified in normal shell
	{
		runit(argv, 0);
	}
	else // otherwise run custom shell
	{
		runshell();
	}

	return 1;
}

