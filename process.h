// like popen, but allowing for non-blocking reads
#ifndef PROCESS_H_
#define PROCESS_H_

typedef struct Process Process;

// execute the given command (like if it was passed to system()).
// returns false on failure
bool process_run(Process *process, char const *command);
// returns the error last error produced, or NULL if there was no error.
char const *process_geterr(Process *process);
// returns:
// -2 on error
// -1 if no data is available right now
// 0 on end of file
// or a positive number indicating the number of bytes read to data (at most size)
long long process_read(Process *process, char *data, size_t size);
// Checks if the process has exited. Returns:
// -1 if the process returned a non-zero exit code, or got a signal.
// 1  if the process exited successfully
// 0  if the process hasn't exited.
// If message is not NULL, it will be set to a description of what happened (e.g. "exited successfully")
int process_check_status(Process *process, char *message, size_t message_size);
// kills process if still running
void process_kill(Process *process);

#endif
