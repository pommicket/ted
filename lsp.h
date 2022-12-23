// @TODO:
// - use document IDs instead of strings (also lets us use real document version numbers)
// - document this and lsp.c.
// - deal with "Save as" (generate didOpen)
// - maximum queue size for requests/responses just in case?
// - delete old sent requests
//    (if the server never sends a response)
// - TESTING: make rust-analyzer-slow (waits 10s before sending response)

typedef u32 DocumentID;

typedef enum {
	LSP_REQUEST,
	LSP_RESPONSE
} LSPMessageType;

typedef struct {
	u32 offset;
} LSPString;

typedef struct {
	u32 line;
	// NOTE: this is the UTF-16 character index!
	u32 character;
} LSPPosition;

typedef struct {
	LSPPosition start;
	LSPPosition end;
} LSPRange;

typedef enum {
	LSP_REQUEST_NONE,
	
	// client-to-server
	LSP_REQUEST_INITIALIZE,
	LSP_REQUEST_INITIALIZED,
	LSP_REQUEST_DID_OPEN,
	LSP_REQUEST_DID_CLOSE,
	LSP_REQUEST_DID_CHANGE,
	LSP_REQUEST_COMPLETION,
	LSP_REQUEST_SHUTDOWN,
	LSP_REQUEST_EXIT,
	
	// server-to-client
	LSP_REQUEST_SHOW_MESSAGE,
	LSP_REQUEST_LOG_MESSAGE,
} LSPRequestType;

typedef struct {
	Language language;
	DocumentID document;
	// freed by lsp_request_free
	char *file_contents;
} LSPRequestDidOpen;

typedef struct {
	DocumentID document;
} LSPRequestDidClose;

// see TextDocumentContentChangeEvent in the LSP spec
typedef struct {
	LSPRange range;
	// new text. will be freed. you can use NULL for the empty string.
	char *text;
} LSPDocumentChangeEvent;

typedef struct {
	DocumentID document;
	LSPDocumentChangeEvent *changes; // dynamic array
} LSPRequestDidChange;

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
	DocumentID document;
	LSPPosition pos;
} LSPDocumentPosition;

typedef struct {
	LSPDocumentPosition position;
} LSPRequestCompletion;

typedef struct {
	// id is set by lsp.c; you shouldn't set it.
	u32 id;
	LSPRequestType type;
	union {
		LSPRequestDidOpen open;
		LSPRequestDidClose close;
		LSPRequestDidChange change;
		LSPRequestCompletion completion;
		// for LSP_SHOW_MESSAGE and LSP_LOG_MESSAGE
		LSPRequestMessage message;
	} data;
} LSPRequest;

typedef enum {
	// LSP doesn't actually define this but this will be used for unrecognized values
	//  (in case they add more symbol kinds in the future)
	LSP_SYMBOL_OTHER = 0,
	
	#define LSP_SYMBOL_KIND_MIN 1
	LSP_SYMBOL_FILE = 1,
	LSP_SYMBOL_MODULE = 2,
	LSB_SYMBOL_NAMESPACE = 3,
	LSP_SYMBOL_PACKAGE = 4,
	LSP_SYMBOL_CLASS = 5,
	LSP_SYMBOL_METHOD = 6,
	LSP_SYMBOL_PROPERTY = 7,
	LSP_SYMBOL_FIELD = 8,
	LSP_SYMBOL_CONSTRUCTOR = 9,
	LSP_SYMBOL_ENUM = 10,
	LSP_SYMBOL_INTERFACE = 11,
	LSP_SYMBOL_FUNCTION = 12,
	LSP_SYMBOL_VARIABLE = 13,
	LSP_SYMBOL_CONSTANT = 14,
	LSP_SYMBOL_STRING = 15,
	LSP_SYMBOL_NUMBER = 16,
	LSP_SYMBOL_BOOLEAN = 17,
	LSP_SYMBOL_ARRAY = 18,
	LSP_SYMBOL_OBJECT = 19,
	LSP_SYMBOL_KEY = 20,
	LSP_SYMBOL_NULL = 21,
	LSP_SYMBOL_ENUMMEMBER = 22,
	LSP_SYMBOL_STRUCT = 23,
	LSP_SYMBOL_EVENT = 24,
	LSP_SYMBOL_OPERATOR = 25,
	LSP_SYMBOL_TYPEPARAMETER = 26,
	#define LSP_SYMBOL_KIND_MAX 26
} LSPSymbolKind;

typedef enum {
	#define LSP_COMPLETION_KIND_MIN 1
	LSP_COMPLETION_TEXT = 1,
	LSP_COMPLETION_METHOD = 2,
	LSP_COMPLETION_FUNCTION = 3,
	LSP_COMPLETION_CONSTRUCTOR = 4,
	LSP_COMPLETION_FIELD = 5,
	LSP_COMPLETION_VARIABLE = 6,
	LSP_COMPLETION_CLASS = 7,
	LSP_COMPLETION_INTERFACE = 8,
	LSP_COMPLETION_MODULE = 9,
	LSP_COMPLETION_PROPERTY = 10,
	LSP_COMPLETION_UNIT = 11,
	LSP_COMPLETION_VALUE = 12,
	LSP_COMPLETION_ENUM = 13,
	LSP_COMPLETION_KEYWORD = 14,
	LSP_COMPLETION_SNIPPET = 15,
	LSP_COMPLETION_COLOR = 16,
	LSP_COMPLETION_FILE = 17,
	LSP_COMPLETION_REFERENCE = 18,
	LSP_COMPLETION_FOLDER = 19,
	LSP_COMPLETION_ENUMMEMBER = 20,
	LSP_COMPLETION_CONSTANT = 21,
	LSP_COMPLETION_STRUCT = 22,
	LSP_COMPLETION_EVENT = 23,
	LSP_COMPLETION_OPERATOR = 24,
	LSP_COMPLETION_TYPEPARAMETER = 25,
	#define LSP_COMPLETION_KIND_MAX 25
} LSPCompletionKind;


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
	// display text for this completion
	LSPString label;
	// text used to filter completions
	LSPString filter_text;
	// more detail for this item, e.g. the signature of a function
	LSPString detail;
	// the edit to be applied when this completion is selected.
	LSPTextEdit text_edit;
	// note: the items are sorted here in this file,
	// so you probably don't need to access this.
	LSPString sort_text;
	// is this function/type/whatever deprecated?
	bool deprecated;
	// type of completion
	LSPCompletionKind kind;
} LSPCompletionItem;

typedef struct {
	// dynamic array
	LSPCompletionItem *items;
} LSPResponseCompletion;

typedef LSPRequestType LSPResponseType;
typedef struct {
	LSPRequest request; // the request which this is a response to
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
	StrHashTable document_ids; // values are u32. they are indices into document_filenames.
	// this is a dynamic array which just keeps growing.
	// but the user isn't gonna open millions of files so it's fine.
	char **document_paths;
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
u32 lsp_document_id(LSP *lsp, const char *path);
void lsp_send_request(LSP *lsp, const LSPRequest *request);
const char *lsp_response_string(const LSPResponse *response, LSPString string);
bool lsp_create(LSP *lsp, const char *analyzer_command);
bool lsp_next_message(LSP *lsp, LSPMessage *message);
void lsp_document_changed(LSP *lsp, const char *document, LSPDocumentChangeEvent change);
void lsp_free(LSP *lsp);
SymbolKind lsp_symbol_kind_to_ted(LSPSymbolKind kind);
SymbolKind lsp_completion_kind_to_ted(LSPCompletionKind kind);
