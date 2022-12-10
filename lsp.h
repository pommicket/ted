// @TODO:
// - document this and lsp.c.
// - maximum queue size for requests/responses just in case?
// - delete old sent requests
//    (if the server never sends a response)
// - TESTING: make rust-analyzer-slow (waits 10s before sending response)

typedef enum {
	LSP_REQUEST,
	LSP_RESPONSE
} LSPMessageType;

typedef enum {
	LSP_NONE,
	
	// client-to-server
	LSP_INITIALIZE,
	LSP_INITIALIZED,
	LSP_OPEN,
	LSP_COMPLETION,
	LSP_SHUTDOWN,
	LSP_EXIT,
	
	// server-to-client
	LSP_SHOW_MESSAGE,
	LSP_LOG_MESSAGE
} LSPRequestType;

typedef struct {
	// buffer language
	Language language;
	// freed by lsp_request_free
	char *path;
	// freed by lsp_request_free
	char *file_contents;
} LSPRequestOpen;

typedef enum {
	ERROR = 1,
	WARNING = 2,
	INFO = 3,
	LOG = 4
} LSPWindowMessageType;

typedef struct {
	LSPWindowMessageType type;
	// freed by lsp_request_free
	char *message;
} LSPRequestMessage;

typedef struct {
	// freed by lsp_request_free
	char *path;
	u32 line;
	// the **UTF-16** "character" offset within the line
	u32 character;
} LSPDocumentPosition;

typedef struct {
	LSPDocumentPosition position;
} LSPRequestCompletion;

typedef struct {
	// id is set by lsp.c; you shouldn't set it.
	u32 id;
	LSPRequestType type;
	union {
		LSPRequestOpen open;
		LSPRequestCompletion completion;
		// for LSP_SHOW_MESSAGE and LSP_LOG_MESSAGE
		LSPRequestMessage message;
	} data;
} LSPRequest;

typedef struct {
	u32 offset;
} LSPString;

typedef struct {
	u32 line;
	u32 character;
} LSPPosition;

typedef struct {
	LSPPosition start;
	LSPPosition end;
} LSPRange;

// see InsertTextFormat in the LSP spec.
typedef enum {
	// plain text
	LSP_TEXT_EDIT_PLAIN = 1,
	// snippet   e.g. "some_method($1, $2)$0"
	LSP_TEXT_EDIT_SNIPPET = 2
} LSPTextEditType;

typedef struct {
	LSPTextEditType type;

	// if set to true, `range` should be ignored
	//  -- this is a completion which uses insertText.
	// how to handle this:
	// "VS Code when code complete is requested in this example
	// `con<cursor position>` and a completion item with an `insertText` of
	// `console` is provided it will only insert `sole`"
	bool at_cursor;
	
	LSPRange range;
	LSPString new_text;
} LSPTextEdit;

typedef struct {
	LSPString label;
	LSPTextEdit text_edit;
	// note: the items are sorted here in this file,
	// so you probably don't need to access this.
	LSPString sort_text;
} LSPCompletionItem;

typedef struct {
	// dynamic array
	LSPCompletionItem *items;
} LSPResponseCompletion;

typedef LSPRequestType LSPResponseType;
typedef struct {
	LSPResponseType type;
	// LSP responses tend to have a lot of strings.
	// to avoid doing a ton of allocations+frees,
	// they're all stored here.
	char *string_data;
	union {
		LSPResponseCompletion completion;
	} data;
} LSPResponse;

typedef struct {
	LSPMessageType type;
	union {
		LSPRequest request;
		LSPResponse response;
	} u;
} LSPMessage;

typedef struct LSP {
	Process process;
	u32 request_id;
	LSPMessage *messages;
	SDL_mutex *messages_mutex;
	LSPRequest *requests_client2server;
	LSPRequest *requests_server2client;
	// we keep track of client-to-server requests
	// so that we can process responses.
	// also fucking rust-analyzer gives "waiting for cargo metadata or cargo check"
	// WHY NOT JUST WAIT UNTIL YOUVE DONE THAT BEFORE SENDING THE INITIALIZE RESPONSE. YOU HAVE NOT FINISHED INITIALIZATION. YOU ARE LYING.
	// YOU GIVE A -32801 ERROR CODE WHICH IS "ContentModified"  -- WHAT THE FUCK? THATS JUST COMPLETLY WRONG
	// so we need to re-send requests in that case.
	LSPRequest *requests_sent;
	SDL_mutex *requests_mutex;
	bool initialized; // has the response to the initialize request been sent?
	SDL_Thread *communication_thread;
	SDL_sem *quit_sem;
	char *received_data; // dynamic array
	SDL_mutex *error_mutex;
	char error[256];
} LSP;

// @TODO: function declarations

// returns true if there's an error.
// returns false and sets error to "" if there's no error.
// if clear = true, the error will be cleared.
// you can set error = NULL, error_size = 0, clear = true to just clear the error
bool lsp_get_error(LSP *lsp, char *error, size_t error_size, bool clear);
void lsp_message_free(LSPMessage *message);
void lsp_send_request(LSP *lsp, const LSPRequest *request);
const char *lsp_response_string(const LSPResponse *response, LSPString string);
bool lsp_create(LSP *lsp, const char *analyzer_command);
bool lsp_next_message(LSP *lsp, LSPMessage *message);
void lsp_free(LSP *lsp);
