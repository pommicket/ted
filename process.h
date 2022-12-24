// like popen, but allowing for non-blocking reads
#ifndef PROCESS_H_
#define PROCESS_H_

typedef struct Process Process;

// zero everything except what you're using
typedef struct {
	bool stdin_blocking;
	bool stdout_blocking;
	bool separate_stderr;
	bool stderr_blocking; // not applicable if separate_stderr is false.
	const char *working_directory;
} ProcessSettings;

// get process ID of this process
int process_get_id(void);
// execute the given command (like if it was passed to system()).
// returns false on failure
bool process_run_ex(Process *proc, const char *command, const ProcessSettings *props);
// like process_run_ex, but with the default settings
bool process_run(Process *process, char const *command);
// returns the error last error produced, or NULL if there was no error.
char const *process_geterr(Process *process);
// write to stdin
// returns:
// -2 on error
// or a non-negative number indicating the number of bytes written.
long long process_write(Process *process, const char *data, size_t size);
// read from stdout+stderr
// returns:
// -2 on error
// -1 if no data is available right now
// 0 on end of file
// or a positive number indicating the number of bytes read to data (at most size)
long long process_read(Process *process, char *data, size_t size);
// like process_read, but reads stderr.
// this function ALWAYS RETURNS -2 if separate_stderr is not specified in the ProcessSettings.
//   if separate_stderr is false, then both stdout and stderr will be sent via process_read.
long long process_read_stderr(Process *process, char *data, size_t size);
// Checks if the process has exited. Returns:
// -1 if the process returned a non-zero exit code, or got a signal.
// 1  if the process exited successfully
// 0  if the process hasn't exited.
// If message is not NULL, it will be set to a description of what happened (e.g. "exited successfully")
int process_check_status(Process *process, char *message, size_t message_size);
// kills process if still running
void process_kill(Process *process);

#endif
