typedef u32 LSPDocumentID;
typedef u32 LSPID;
typedef u32 LSPRequestID;

typedef struct {
	u32 line;
	// NOTE: this is the UTF-16 character index!
	u32 character;
} LSPPosition;

typedef struct {
	LSPDocumentID document;
	LSPPosition pos;
} LSPDocumentPosition;

typedef enum {
	LSP_REQUEST,
	LSP_RESPONSE
} LSPMessageType;

typedef struct {
	u32 offset;
} LSPString;

typedef struct {
	LSPPosition start;
	LSPPosition end;
} LSPRange;

typedef struct {
	LSPDocumentID document;
	LSPRange range;
} LSPLocation;

typedef enum {
	LSP_REQUEST_NONE,
	
	// client-to-server
	LSP_REQUEST_INITIALIZE, // initialize
	LSP_REQUEST_INITIALIZED, // initialized
	// workspace/didChangeConfiguration with parameters specifically for jdtls.
	// we need this because annoyingly jdtls refuses to give signature help
	// unless you specifically configure it to do that
	LSP_REQUEST_JDTLS_CONFIGURATION,
	LSP_REQUEST_SHUTDOWN, // shutdown
	LSP_REQUEST_EXIT, // exit
	LSP_REQUEST_DID_OPEN, // textDocument/didOpen
	LSP_REQUEST_DID_CLOSE, // textDocument/didClose
	LSP_REQUEST_DID_CHANGE, // textDocument/didChange
	LSP_REQUEST_COMPLETION, // textDocument/completion
	LSP_REQUEST_SIGNATURE_HELP, // textDocument/signatureHelp
	LSP_REQUEST_HOVER, // textDocument/hover
	LSP_REQUEST_DEFINITION, // textDocument/definition
	LSP_REQUEST_WORKSPACE_SYMBOLS, // workspace/symbol
	LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS, // workspace/didChangeWorkspaceFolders
	
	// server-to-client
	LSP_REQUEST_SHOW_MESSAGE, // window/showMessage and window/showMessageRequest
	LSP_REQUEST_LOG_MESSAGE, // window/logMessage
	LSP_REQUEST_WORKSPACE_FOLDERS, // workspace/workspaceFolders - NOTE: this is handled directly in lsp-parse.c (because it only needs information from the LSP struct)
} LSPRequestType;

typedef struct {
	Language language;
	LSPDocumentID document;
	// freed by lsp_request_free
	char *file_contents;
} LSPRequestDidOpen;

typedef struct {
	LSPDocumentID document;
} LSPRequestDidClose;

// see TextDocumentContentChangeEvent in the LSP spec
typedef struct {
	LSPRange range;
	// new text. will be freed. you can use NULL for the empty string.
	char *text;
} LSPDocumentChangeEvent;

typedef struct {
	LSPDocumentID document;
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


// these triggers are used for completion context and signature help context.
#define LSP_TRIGGER_NONE 0 // not actually defined in LSP spec
#define LSP_TRIGGER_INVOKED 1
#define LSP_TRIGGER_CHARACTER 2
#define LSP_TRIGGER_INCOMPLETE 3
#define LSP_TRIGGER_CONTENT_CHANGE 3
typedef u8 LSPCompletionTriggerKind;
typedef u8 LSPSignatureHelpTriggerKind;

typedef struct {
	LSPCompletionTriggerKind trigger_kind;
	char trigger_character[5];
} LSPCompletionContext;

typedef struct {
	LSPDocumentPosition position;
	LSPCompletionContext context;
} LSPRequestCompletion;

typedef struct {
	LSPDocumentPosition position;
} LSPRequestSignatureHelp;

typedef struct {
	LSPDocumentPosition position;
} LSPRequestHover;

typedef struct {
	LSPDocumentPosition position;
} LSPRequestDefinition;

typedef struct {
	char *query;
} LSPRequestWorkspaceSymbols;

typedef struct {
	LSPDocumentID *removed; // dynamic array
	LSPDocumentID *added; // dynamic array
} LSPRequestDidChangeWorkspaceFolders;

typedef struct {
	u32 id;
	LSPRequestType type;
	char *id_string; // if not NULL, this is the ID (only for server-to-client messages; we always use integer IDs)
	// one member of this union is set depending on `type`.
	union {	
		LSPRequestDidOpen open;
		LSPRequestDidClose close;
		LSPRequestDidChange change;
		LSPRequestCompletion completion;
		LSPRequestSignatureHelp signature_help;
		LSPRequestHover hover;
		LSPRequestDefinition definition;
		LSPRequestWorkspaceSymbols workspace_symbols;
		// LSP_REQUEST_SHOW_MESSAGE or LSP_REQUEST_LOG_MESSAGE
		LSPRequestMessage message;
		LSPRequestDidChangeWorkspaceFolders change_workspace_folders;
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
	// documentation for this item (typically from a doc comment)
	LSPString documentation;
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
	bool is_complete;
	// dynamic array
	LSPCompletionItem *items;
} LSPResponseCompletion;

typedef struct {
	LSPString label;
	// NOTE: LSP gives us parameter information for *all*
	// parameters, but we only really need it for the active parameter.
	
	// (UTF-16) indices into `label` indicating which
	// part of it should be highlighted for the active parameter
	u16 active_start;
	u16 active_end;
} LSPSignatureInformation;

typedef struct {
	// NOTE: the "active" signature will be the first one
	// in this array.
	LSPSignatureInformation *signatures;
} LSPResponseSignatureHelp;

typedef struct {
	// the range of text to highlight
	LSPRange range;
	// little tool tip to show
	LSPString contents;
} LSPResponseHover;

typedef struct {
	// where the symbol is defined (dynamic array)
	LSPLocation *locations;
} LSPResponseDefinition;

// SymbolInformation in the LSP spec
typedef struct {
	LSPString name;
	LSPSymbolKind kind;
	bool deprecated;
	LSPLocation location;
} LSPSymbolInformation;

typedef struct {
	LSPSymbolInformation *symbols;
} LSPResponseWorkspaceSymbols;

typedef LSPRequestType LSPResponseType;
typedef struct {
	LSPRequest request; // the request which this is a response to
	char *error; // if not NULL, the data field will just be zeroed
	// LSP responses tend to have a lot of strings.
	// to avoid doing a ton of allocations+frees,
	// they're all stored here.
	char *string_data;
	// one of these is filled based on request.type
	union {
		LSPResponseCompletion completion;
		LSPResponseSignatureHelp signature_help;
		LSPResponseHover hover;
		LSPResponseDefinition definition;
	} data;
} LSPResponse;

typedef struct {
	LSPMessageType type;
	union {
		LSPRequest request;
		LSPResponse response;
	} u;
} LSPMessage;

typedef struct {
	char *path;
	u32 version_number; // for LSP
} LSPDocumentData;

typedef struct {
	bool signature_help_support;
	bool completion_support;
	bool hover_support;
	bool definition_support;
	bool workspace_symbols_support;
	// support for multiple root folders
	// sadly, as of me writing this, clangd and rust-analyzer don't support this
	// (but jdtls and gopls do)
	bool workspace_folders_support;
} LSPCapabilities;

typedef struct LSP {
	// thread safety is important here!
	// every member should either be indented to indicate which mutex controls it,
	// or have a comment explaining why it doesn't need one

	// A unique ID number for this LSP.
	// thread-safety: only set once in lsp_create.
	LSPID id;
	
	// The server process
	// thread-safety: created in lsp_create, then only accessed by the communication thread
	Process process;
	
	SDL_mutex *document_mutex;
		// for our purposes, folders are "documents"
		// the spec kinda does this too: WorkspaceFolder has a `uri: DocumentUri` member.
		StrHashTable document_ids; // values are u32. they are indices into document_data.
		// this is a dynamic array which just keeps growing.
		// but the user isn't gonna open millions of files so it's fine.
		LSPDocumentData *document_data;
	SDL_mutex *messages_mutex;
		LSPMessage *messages_server2client;
		LSPMessage *messages_client2server;
		// we keep track of client-to-server requests
		// so that we can process responses.
		// this also lets us re-send requests if that's ever necessary.
		LSPRequest *requests_sent;
	// has the response to the initialize request been sent?
	// thread-safety: this is atomic. it starts out false, and only gets set to true once
	//                (when the initialize response is received)
	_Atomic bool initialized;
	// thread-safety: only set once in lsp_create.
	Language language;
	SDL_Thread *communication_thread;
	SDL_sem *quit_sem;
	// thread-safety: only accessed in communication thread
	char *received_data; // dynamic array
	// thread-safety: in the communication thread, we fill this in, then set `initialized = true`.
	//                after that, this never changes.
	//                never accessed in main thread before `initialized = true`.
	LSPCapabilities capabilities;
	// thread-safety: same as `capabilities`
	char32_t *completion_trigger_chars; // dynamic array
	// thread-safety: same as `capabilities`
	char32_t *signature_help_trigger_chars; // dynamic array
	// thread-safety: same as `capabilities`
	char32_t *signature_help_retrigger_chars; // dynamic array
	SDL_mutex *workspace_folders_mutex;
		LSPDocumentID *workspace_folders; // dynamic array of root directories of LSP workspace folders
	SDL_mutex *error_mutex;
		char error[256];
} LSP;

// returns true if there's an error.
// returns false and sets error to "" if there's no error.
// if clear = true, the error will be cleared.
// you can set error = NULL, error_size = 0, clear = true to just clear the error
bool lsp_get_error(LSP *lsp, char *error, size_t error_size, bool clear);
void lsp_message_free(LSPMessage *message);
u32 lsp_document_id(LSP *lsp, const char *path);
// returned pointer lives exactly as long as lsp.
const char *lsp_document_path(LSP *lsp, LSPDocumentID id);
// returns the ID of the sent request, or 0 if the request is not supported by the LSP
// don't free the contents of this request (even on failure)! let me handle it!
LSPRequestID lsp_send_request(LSP *lsp, LSPRequest *request);
// don't free the contents of this response! let me handle it!
void lsp_send_response(LSP *lsp, LSPResponse *response);
const char *lsp_response_string(const LSPResponse *response, LSPString string);
LSP *lsp_create(const char *root_dir, Language language, const char *analyzer_command);
// try to add a new "workspace folder" to the lsp.
// IMPORTANT: only call this if lsp->initialized is true
//            (if not we don't yet know whether the server supports workspace folders)
// returns true on success or if new_root_dir is already contained in a workspace folder for this LSP.
// if this fails (i.e. if the LSP does not have workspace support), create a new LSP
// with root directory `new_root_dir`.
bool lsp_try_add_root_dir(LSP *lsp, const char *new_root_dir);
// report that this document has changed
void lsp_document_changed(LSP *lsp, const char *document, LSPDocumentChangeEvent change);
bool lsp_next_message(LSP *lsp, LSPMessage *message);
bool lsp_position_eq(LSPPosition a, LSPPosition b);
bool lsp_document_position_eq(LSPDocumentPosition a, LSPDocumentPosition b);
LSPDocumentPosition lsp_location_start_position(LSPLocation location);
LSPDocumentPosition lsp_location_end_position(LSPLocation location);
void lsp_free(LSP *lsp);
