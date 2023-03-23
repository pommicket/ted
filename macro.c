#include "ted.h"

void macro_start_recording(Ted *ted, u32 index) {
	if (index >= TED_MACRO_MAX) return;
	if (ted->executing_macro) return;
	ted->recording_macro = &ted->macros[index];
}

void macro_stop_recording(Ted *ted) {
	ted->recording_macro = NULL;
}

void macro_add(Ted *ted, Command command, CommandArgument argument) {
	if (!ted->recording_macro) return;
	if (command == CMD_MACRO_EXECUTE || command == CMD_MACRO_RECORD || command == CMD_MACRO_STOP)
		return;
	if (argument.string)
		argument.string = str_dup(argument.string);
	Action action = {
		.command = command,
		.argument = argument
	};
	arr_add(ted->recording_macro->actions, action);
}

void macro_execute(Ted *ted, u32 index) {
	if (index >= TED_MACRO_MAX) return;
	Macro *macro = &ted->macros[index];
	if (ted->recording_macro == macro) {
		// don't allow running a macro while it's being recorded
		return;
	}
	
	ted->executing_macro = true;
	CommandContext context = {0};
	context.from_macro = true;
	arr_foreach_ptr(macro->actions, Action, act) {
		command_execute_ex(ted, act->command, act->argument, context);
	}
	ted->executing_macro = false;
}

void macros_free(Ted *ted) {
	for (int i = 0; i < TED_MACRO_MAX; ++i) {
		Macro *macro = &ted->macros[i];
		arr_foreach_ptr(macro->actions, Action, act) {
			free((char *)act->argument.string);
		}
		arr_free(macro->actions);
	}
}
