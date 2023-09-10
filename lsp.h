/// \file
/// functions for dealing with LSP (Language Server Protocol) servers.
///
/// don't assume any of the public functions defined here are thread-safe.

#ifndef LSP_H_
#define LSP_H_

#include "base.h"
#include "ds.h"
#include "os.h"
#include "util.h"

/// an ID specific to a path. a document's ID is never 0 (thanks to lsp_create).
typedef u32 LSPDocumentID;
/// ID of an LSP server. a server's ID is never 0.
typedef u32 LSPID;
/// a request ID. this is unique across all servers. a request's ID is never 0.
typedef u32 LSPRequestID;
typedef struct SDL_mutex *LSPMutex;
typedef struct SDL_semaphore *LSPSemaphore;
typedef struct SDL_Thread *LSPThread;

/// a struct for keeping track of a LSP server ID and a request ID
typedef struct {
	LSPID lsp;
	LSPRequestID id;
} LSPServerRequestID;

/// `interface Position` in the LSP spec
typedef struct {
	u32 line;
	// NOTE: this is the UTF-16 character index!
	u32 character;
} LSPPosition;

/// `interface TextDocumentPositionParams` in the LSP spec
typedef struct {
	LSPDocumentID document;
	LSPPosition pos;
} LSPDocumentPosition;

typedef enum {
	LSP_REQUEST,
	LSP_RESPONSE
} LSPMessageType;

/// A string in a `LSPResponse`
typedef struct {
	// offset into string_data
	u32 offset;
} LSPString;

/// `interface Range` in the LSP spec
typedef struct {
	LSPPosition start;
	LSPPosition end;
} LSPRange;

/// `interface Location` in the LSP spec
typedef struct {
	LSPDocumentID document;
	LSPRange range;
} LSPLocation;

typedef enum {
	LSP_REQUEST_NONE,
	
	// client-to-server
	LSP_REQUEST_INITIALIZE, //< initialize
	LSP_REQUEST_INITIALIZED, //< initialized
	LSP_REQUEST_CANCEL, //< $/cancelRequest
	LSP_REQUEST_CONFIGURATION, //< workspace/didChangeConfiguration
	LSP_REQUEST_SHUTDOWN, //< shutdown
	LSP_REQUEST_EXIT, //< exit
	LSP_REQUEST_DID_OPEN, //< textDocument/didOpen
	LSP_REQUEST_DID_CLOSE, //< textDocument/didClose
	LSP_REQUEST_DID_CHANGE, //< textDocument/didChange
	LSP_REQUEST_COMPLETION, //< textDocument/completion
	LSP_REQUEST_SIGNATURE_HELP, //< textDocument/signatureHelp
	LSP_REQUEST_HOVER, //< textDocument/hover
	LSP_REQUEST_DEFINITION, //< textDocument/definition
	LSP_REQUEST_DECLARATION, //< textDocument/declaration
	LSP_REQUEST_TYPE_DEFINITION, //< textDocument/typeDefinition
	LSP_REQUEST_IMPLEMENTATION, //< textDocument/implementation
	LSP_REQUEST_HIGHLIGHT, //< textDocument/documentHighlight
	LSP_REQUEST_REFERENCES, //< textDocument/references
	LSP_REQUEST_RENAME, //< textDocument/rename
	LSP_REQUEST_DOCUMENT_LINK, //< textDocument/documentLink
	LSP_REQUEST_FORMATTING, //< textDocument/formatting
	LSP_REQUEST_RANGE_FORMATTING, //< textDocument/rangeFormatting
	LSP_REQUEST_WORKSPACE_SYMBOLS, //< workspace/symbol
	LSP_REQUEST_DID_CHANGE_WORKSPACE_FOLDERS, //< workspace/didChangeWorkspaceFolders
	// server-to-client
	LSP_REQUEST_SHOW_MESSAGE, //< window/showMessage and window/showMessageRequest
	LSP_REQUEST_LOG_MESSAGE, //< window/logMessage
	LSP_REQUEST_WORKSPACE_FOLDERS, //< workspace/workspaceFolders - NOTE: this is handled directly in lsp-parse.c (because it only needs information from the LSP struct)
	LSP_REQUEST_PUBLISH_DIAGNOSTICS, //< textDocument/publishDiagnostics
} LSPRequestType;

typedef enum {
	LSP_ERROR_PARSE = -32700,
	LSP_ERROR_INVALID_REQUEST = -32600,
	LSP_ERROR_METHOD_NOT_FOUND = -32601,
	LSP_ERROR_INVALID_PARAMS = -32602,
	LSP_ERROR_INTERNAL = -32603,
	LSP_ERROR_SERVER_NOT_INITIALIZED = -32002,
	LSP_ERROR_UNKNOWN_CODE = -32001,
	LSP_ERROR_REQUEST_FAILED = -32803,
	LSP_ERROR_SERVER_CANCELLED = -32802,
	LSP_ERROR_CONTENT_MODIFIED = -32801,
	LSP_ERROR_REQUEST_CANCELLED = -32800,
} LSPError;

typedef struct {
	LSPRequestID id;
} LSPRequestCancel;

typedef struct {
	u64 language;
	LSPDocumentID document;
	LSPString file_contents;
} LSPRequestDidOpen;

typedef struct {
	LSPDocumentID document;
} LSPRequestDidClose;

// see TextDocumentContentChangeEvent in the LSP spec
typedef struct {
	LSPRange range;
	/// if `false`, \ref text refers to the whole document contents after the change.
	bool use_range;
	/// new text.
	LSPString text;
} LSPDocumentChangeEvent;

typedef struct {
	LSPDocumentID document;
	LSPDocumentChangeEvent *changes; // dynamic array
} LSPRequestDidChange;

typedef enum {
	LSP_WINDOW_MESSAGE_ERROR = 1,
	LSP_WINDOW_MESSAGE_WARNING = 2,
	LSP_WINDOW_MESSAGE_INFO = 3,
	LSP_WINDOW_MESSAGE_LOG = 4
} LSPWindowMessageType;

typedef struct {
	LSPWindowMessageType type;
	LSPString message;
} LSPRequestMessage;

typedef enum {
#define LSP_DIAGNOSTIC_SEVERITY_MIN 1
	LSP_DIAGNOSTIC_SEVERITY_ERROR = 1,
	LSP_DIAGNOSTIC_SEVERITY_WARNING = 2,
	LSP_DIAGNOSTIC_SEVERITY_INFORMATION = 3,
	LSP_DIAGNOSTIC_SEVERITY_HINT = 4,
#define LSP_DIAGNOSTIC_SEVERITY_MAX 4
} LSPDiagnosticSeverity;

typedef struct {
	LSPRange range;
	LSPDiagnosticSeverity severity;
	LSPString code;
	LSPString message;
	/// URI to description of code
	/// e.g. for Rust's E0621, this would be https://doc.rust-lang.org/error_codes/E0621.html
	LSPString code_description_uri;
} LSPDiagnostic;

typedef struct {
	LSPDocumentID document;
	LSPDiagnostic *diagnostics; // dynamic array
} LSPRequestPublishDiagnostics;


/// these triggers are used for completion context and signature help context.
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
	LSPDocumentPosition position;
} LSPRequestHighlight;

typedef struct {
	LSPDocumentPosition position;
	// include the declaration of the symbol as a reference
	bool include_declaration;
} LSPRequestReferences;

typedef struct {
	LSPDocumentID document;
} LSPRequestDocumentLink;

typedef struct {
	// string to filter the symbols by
	LSPString query;
} LSPRequestWorkspaceSymbols;

typedef struct {
	LSPDocumentPosition position;
	LSPString new_name;
} LSPRequestRename;

typedef struct {
	LSPDocumentID *removed; // dynamic array
	LSPDocumentID *added; // dynamic array
} LSPRequestDidChangeWorkspaceFolders;

typedef struct {
	// this string should be valid JSON.
	LSPString settings;
} LSPRequestConfiguration;

typedef struct {
	LSPDocumentID document;
	u8 tab_width;
	bool indent_with_spaces;
	bool use_range;
	/// range to format
	///
	/// only applicable if `use_range` is `true`.
	LSPRange range;
} LSPRequestFormatting;

typedef struct {
	LSPMessageType type;
	/// LSP requests/responses tend to have a lot of strings.
	/// to avoid doing a ton of allocations+frees,
	/// they're all stored here.
	char *string_data;
} LSPMessageBase;

/// an LSP request or notification
typedef struct {
	LSPMessageBase base;
	LSPRequestID id;
	LSPRequestType type;
	LSPString id_string; // if non-empty, this is the ID (only for server-to-client messages; we always use integer IDs)
	// one member of this union is set depending on `type`.
	union {	
		LSPRequestCancel cancel;
		LSPRequestDidOpen open;
		LSPRequestDidClose close;
		LSPRequestDidChange change;
		LSPRequestConfiguration configuration;
		LSPRequestCompletion completion;
		LSPRequestSignatureHelp signature_help;
		LSPRequestHover hover;
		// LSP_REQUEST_DEFINITION, LSP_REQUEST_DECLARATION, LSP_REQUEST_TYPE_DEFINITION, or LSP_REQUEST_IMPLEMENTATION
		LSPRequestDefinition definition;
		LSPRequestHighlight highlight;
		LSPRequestReferences references;
		LSPRequestWorkspaceSymbols workspace_symbols;
		// LSP_REQUEST_SHOW_MESSAGE or LSP_REQUEST_LOG_MESSAGE
		LSPRequestMessage message;
		LSPRequestDidChangeWorkspaceFolders change_workspace_folders;
		LSPRequestRename rename;
		LSPRequestDocumentLink document_link;
		LSPRequestPublishDiagnostics publish_diagnostics;
		// LSP_REQUEST_FORMATTING and LSP_REQUEST_RANGE_FORMATTING
		LSPRequestFormatting formatting;
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

// interface TextEdit in the LSP spec
typedef struct {	
	LSPRange range;
	LSPString new_text;
} LSPTextEdit;

// see InsertTextFormat in the LSP spec.
typedef enum {
	// plain text
	LSP_COMPLETION_EDIT_PLAIN = 1,
	// snippet   e.g. "some_method($1, $2)$0"
	LSP_COMPLETION_EDIT_SNIPPET = 2
} LSPCompletionEditType;


// interface CompletionItem in the LSP spec
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
	// type for text_edit
	LSPCompletionEditType edit_type;
	// if set to true, `text_edit.range` should be ignored
	//  -- this is a completion which uses insertText.
	// how to handle this:
	// "VS Code when code complete is requested in this example
	// `con<cursor position>` and a completion item with an `insertText` of
	// `console` is provided it will only insert `sole`"
	bool at_cursor;
	// note: the items are sorted here in this file,
	// so you probably don't need to access this.
	LSPString sort_text;
	// is this function/type/whatever deprecated?
	bool deprecated;
	// type of completion
	LSPCompletionKind kind;
} LSPCompletionItem;

typedef struct {
	// should completions be re-requested when more characters are typed?
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

typedef enum {
#define LSP_HIGHLIGHT_MIN 1
	LSP_HIGHLIGHT_TEXT = 1,
	LSP_HIGHLIGHT_READ = 2,
	LSP_HIGHLIGHT_WRITE = 3,
#define LSP_HIGHLIGHT_MAX 3
} LSPHighlightKind;

// interface DocumentHighlight in the LSP spec
typedef struct {
	LSPRange range;
	LSPHighlightKind kind;
} LSPHighlight;

typedef struct {
	LSPHighlight *highlights;
} LSPResponseHighlight;

typedef struct {
	// these will be sorted by path (alphabetically), then by line number
	LSPLocation *locations;
} LSPResponseReferences;

typedef enum {
	#define LSP_SYMBOL_TAG_MIN 1
	LSP_SYMBOL_TAG_DEPRECATED = 1
	#define LSP_SYMBOL_TAG_MAX 1
} LSPSymbolTag;

// SymbolInformation in the LSP spec
typedef struct {
	LSPString name;
	LSPSymbolKind kind;
	bool deprecated;
	LSPLocation location;
	// the "symbol containing this symbol"
	// e.g. multiple classes might have a "foo" method, so this can be used to distinguish them.
	LSPString container;
} LSPSymbolInformation;

typedef struct {
	LSPSymbolInformation *symbols;
} LSPResponseWorkspaceSymbols;

typedef enum {
	// yes, we do need to store multiple edits in a single workspace change;
	//   doing a workspace change with TextEdit[] t1
	//   followed by a workspace change with TextEdit[] t2
	//   is different from a workspace change with t1+t2
	// (see https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textEditArray,
	//      https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentEdit)
	// because microsoft is a bunch of idiots
	LSP_CHANGE_EDITS = 1,
	LSP_CHANGE_CREATE,
	LSP_CHANGE_RENAME,
	LSP_CHANGE_DELETE
} LSPWorkspaceChangeType;

typedef struct {
	LSPDocumentID document;
	LSPTextEdit *edits;
} LSPWorkspaceChangeEdit;

typedef struct {
	LSPDocumentID document;
	bool overwrite;
	bool ignore_if_exists;
} LSPWorkspaceChangeCreate;

typedef struct {
	LSPDocumentID old;
	LSPDocumentID new;
	bool overwrite;
	bool ignore_if_exists;
} LSPWorkspaceChangeRename;

typedef struct {
	LSPDocumentID document;
	bool recursive;
	bool ignore_if_not_exists;
} LSPWorkspaceChangeDelete;

// this doesn't exist in the LSP spec. it represents
// a single change from a WorkspaceEdit.
typedef struct {
	LSPWorkspaceChangeType type;
	union {
		LSPWorkspaceChangeEdit edit;
		LSPWorkspaceChangeCreate create;
		LSPWorkspaceChangeRename rename;
		LSPWorkspaceChangeDelete delete;
	} data;
} LSPWorkspaceChange;

typedef struct {
	LSPWorkspaceChange *changes;
} LSPWorkspaceEdit;
typedef LSPWorkspaceEdit LSPResponseRename;

typedef struct {
	LSPRange range;
	LSPString target;
	LSPString tooltip;
} LSPDocumentLink;

typedef struct {
	LSPDocumentLink *links;
} LSPResponseDocumentLink;

typedef struct {
	LSPTextEdit *edits;
} LSPResponseFormatting;

typedef struct {
	LSPMessageBase base;
	/// the request which this is a response to
	LSPRequest request;
	/// if not NULL, the data field will just be zeroed
	LSPString error;
	/// one of these is filled based on request.type
	union {
		LSPResponseCompletion completion;
		LSPResponseSignatureHelp signature_help;
		LSPResponseHover hover;
		/// `LSP_REQUEST_DEFINITION`, `LSP_REQUEST_DECLARATION`, `LSP_REQUEST_TYPE_DEFINITION`, or `LSP_REQUEST_IMPLEMENTATION`
		LSPResponseDefinition definition;
		LSPResponseWorkspaceSymbols workspace_symbols;
		LSPResponseRename rename;
		LSPResponseHighlight highlight;
		LSPResponseReferences references;
		LSPResponseDocumentLink document_link;
		/// `LSP_REQUEST_FORMATTING` or `LSP_REQUEST_RANGE_FORMATTING`
		LSPResponseFormatting formatting;
	} data;
} LSPResponse;

// *technically* using unions this way is UB,
// but Xlib/SDL/others do it so compilers've gotta deal with it.
typedef union {
	LSPMessageType type;
	LSPMessageBase base;
	LSPRequest request;
	LSPResponse response;
} LSPMessage;

typedef struct {
	char *path;
	u32 version_number; // for LSP
} LSPDocumentData;

typedef struct {
	/// send didChange?
	bool sync_support;
	/// can didChange notifications have partial changes?
	bool incremental_sync_support;
	/// send didOpen/didClose?
	bool open_close_support;
	bool signature_help_support;
	bool completion_support;
	bool hover_support;
	bool definition_support;
	bool declaration_support;
	bool implementation_support;
	bool type_definition_support;
	bool workspace_symbols_support;
	bool highlight_support;
	// support for multiple root folders
	// sadly, as of me writing this, clangd and rust-analyzer don't support this
	// (but jdtls and gopls do)
	bool workspace_folders_support;
	bool rename_support;
	bool references_support;
	bool document_link_support;
	bool formatting_support;
	bool range_formatting_support;
} LSPCapabilities;

typedef struct LSP LSP;

/// arguments to \ref lsp_create, but in `struct` form because
/// there are so many of them.
typedef struct {
	/// root directory
	const char *root_dir;
	/// command to run to start server (set to `NULL` if LSP is assumed to already be running)
	const char *command;
	/// port which server is listening on (set to 0 for LSP over stdio)
	u16 port;
	/// configuration JSON
	const char *configuration;
	/// log file, or `NULL` to disable logging
	FILE *log;
	/// see `LSP::send_delay`
	double send_delay;
} LSPSetup;

/// Start up an LSP server.
LSP *lsp_create(const LSPSetup *setup);
/// get unique ID associated with this server.
u32 lsp_get_id(const LSP *lsp);
/// has the server been initialized?
bool lsp_is_initialized(LSP *lsp);
/// \returns the \ref LSPSetup::command value passed into \ref lsp_create
const char *lsp_get_command(LSP *lsp);
/// \returns the \ref LSPSetup::port value passed into \ref lsp_create
u16 lsp_get_port(LSP *lsp);
/// has the server exited?
bool lsp_has_exited(LSP *lsp);
/// Assiociate `id` with the LSP language identifier `lsp_identifier` (see https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#-textdocumentitem-)
void lsp_register_language(u64 id, const char *lsp_identifier);
// returns true if there's an error.
// returns false and sets error to "" if there's no error.
// if clear = true, the error will be cleared.
// you can set error = NULL, error_size = 0, clear = true to just clear the error
bool lsp_get_error(LSP *lsp, char *error, size_t error_size, bool clear);
void lsp_message_free(LSPMessage *message);
u32 lsp_document_id(LSP *lsp, const char *path);
// returned pointer lives as long as lsp.
const char *lsp_document_path(LSP *lsp, LSPDocumentID id);
// returns the ID of the sent request, or (LSPServerRequestID){0} if the request is not supported by the LSP
// don't free the contents of this request (even on failure)! let me handle it!
LSPServerRequestID lsp_send_request(LSP *lsp, LSPRequest *request);
// send a $/cancelRequest notification
// if id = 0, nothing will happen.
void lsp_cancel_request(LSP *lsp, LSPRequestID id);
// don't free the contents of this response! let me handle it!
void lsp_send_response(LSP *lsp, LSPResponse *response);
const char *lsp_response_string(const LSPResponse *response, LSPString string);
const char *lsp_request_string(const LSPRequest *request, LSPString string);
/// low-level API for allocating message strings.
///
/// sets `*string` to the LSPString, and returns a pointer which you can write the string to.
/// the returned pointer will be zeroed up to and including [len].
char *lsp_message_alloc_string(LSPMessageBase *message, size_t len, LSPString *string);
LSPString lsp_message_add_string32(LSPMessageBase *message, String32 string);
LSPString lsp_request_add_string(LSPRequest *request, const char *string);
LSPString lsp_response_add_string(LSPResponse *response, const char *string);
bool lsp_string_is_empty(LSPString string);
// try to add a new "workspace folder" to the lsp.
// IMPORTANT: only call this if lsp->initialized is true
//            (if not we don't yet know whether the server supports workspace folders)
// returns true on success or if new_root_dir is already contained in a workspace folder for this LSP.
// if this fails (i.e. if the LSP does not have workspace support), create a new LSP
// with root directory `new_root_dir`.
bool lsp_try_add_root_dir(LSP *lsp, const char *new_root_dir);
// is this path in the LSP's workspace folders?
bool lsp_covers_path(LSP *lsp, const char *path);
// get next message from server
bool lsp_next_message(LSP *lsp, LSPMessage *message);
/// returns `-1` if `a` comes before `b`, 0 if `a` and `b` are equal, and `1` if `a` comes after `b`
int lsp_position_cmp(LSPPosition a, LSPPosition b);
/// returns `true` if `a` and `b` are equal
bool lsp_position_eq(LSPPosition a, LSPPosition b);
/// returns `true` if `a` and `b` overlap
bool lsp_ranges_overlap(LSPRange a, LSPRange b);
bool lsp_document_position_eq(LSPDocumentPosition a, LSPDocumentPosition b);
/// does this server support incremental synchronization
///
/// see https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_synchronization
/// for more info.
bool lsp_has_incremental_sync_support(LSP *lsp);
/// get dynamic array of completion trigger characters.
const uint32_t *lsp_completion_trigger_chars(LSP *lsp);
/// get dynamic array of signature help trigger characters.
const uint32_t *lsp_signature_help_trigger_chars(LSP *lsp);
/// get dynamic array of signature help retrigger characters.
const uint32_t *lsp_signature_help_retrigger_chars(LSP *lsp);
// get the start of location's range as a LSPDocumentPosition
LSPDocumentPosition lsp_location_start_position(LSPLocation location);
// get the end of location's range as a LSPDocumentPosition
LSPDocumentPosition lsp_location_end_position(LSPLocation location);
void lsp_free(LSP *lsp);
// call this to free any global resources used by lsp*.c
// not strictly necessary, but prevents valgrind errors & stuff.
// make sure you call lsp_free on every LSP you create before calling this.
void lsp_quit(void);

#endif // LSP_H_

#if defined LSP_INTERNAL && !defined LSP_INTERNAL_H_
#define LSP_INTERNAL_H_

struct LSP {
	// thread safety is important here!
	// every member should either be indented to indicate which mutex controls it,
	// or have a comment explaining why it doesn't need one

	// A unique ID number for this LSP.
	// thread-safety: only set once in \ref lsp_create.
	LSPID id;
	
	// thread-safety: set once in \ref lsp_create, then only used by communication thread
	FILE *log;
	
	// The server process. May be NULL if the process isn't started by ted.
	//
	// thread-safety: created in \ref lsp_create, then only accessed by the communication thread
	Process *process;
	// Socket for communicating with server. Maybe be NULL if communication is done over stdio.
	//
	// thread-safety: only accessed in communication thread
	// at least one of `process` and `socket` must be non-null
	Socket *socket;
	// port used for communication
	//
	// this will be zero iff communication is done over stdio
	// thread-safety: only set once in \ref lsp_create
	u16 port;
	
	/// delay before sending requests.
	///
	/// this exists for servers which don't support `$/cancelRequest`
	/// to avoid flooding them with requests which they can't keep up with.
	///
	/// thread-safety: only set once in \ref lsp_create
	double send_delay;

	LSPMutex document_mutex;
		// for our purposes, folders are "documents"
		// the spec kinda does this too: WorkspaceFolder has a `uri: DocumentUri` member.
		StrHashTable document_ids; // values are u32. they are indices into document_data.
		// this is a dynamic array which just keeps growing.
		// but the user isn't gonna open millions of files so it's fine.
		LSPDocumentData *document_data;
	LSPMutex messages_mutex;
		LSPMessage *messages_server2client;
		LSPMessage *messages_client2server;
		// we keep track of client-to-server requests
		// so that we can process responses.
		// this also lets us re-send requests if that's ever necessary.
		LSPRequest *requests_sent;
	// has the response to the initialize request been sent?
	// thread-safety: this starts out false, and only gets set to true once
	//                (when the initialize response is received)
	bool initialized;
	// has the LSP server exited?
	// thread-safety: this starts out false, and only gets set to true once (when the server exits)
	bool exited;
	// thread-safety: only set once in lsp_create.
	char *command;
	// this is set in lsp_create, then later set to NULL when we send over the configuration (after the initialized notification).
	// thread-safety: set once in lsp_create, then only acessed once in communication thread.
	char *configuration_to_send;
	LSPThread communication_thread;
	LSPSemaphore quit_sem;
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
	LSPMutex workspace_folders_mutex;
		// dynamic array of root directories of LSP workspace folders
		LSPDocumentID *workspace_folders;
	LSPMutex error_mutex;
		char error[512];
};

#include "sdl-inc.h"

#define lsp_set_error(lsp, ...) do {\
		SDL_LockMutex(lsp->error_mutex);\
		strbuf_printf(lsp->error, __VA_ARGS__);\
		SDL_UnlockMutex(lsp->error_mutex);\
	} while (0)

// a string
typedef struct {
	u32 pos;
	u32 len;
} JSONString;

typedef struct JSONValue JSONValue;

typedef struct {
	u32 len;
	// this is an index into the values array
	//   values[items..items+len] store the names
	//   values[items+len..items+2*len] store the values
	u32 items;
} JSONObject;

typedef struct {
	u32 len;
	// this is an index into the values array
	//    values[elements..elements+len] are the elements
	u32 elements;
} JSONArray;

typedef enum {
	// note: json doesn't actually include undefined.
	// this is only for returning things from json_get etc.
	JSON_UNDEFINED,
	JSON_NULL,
	JSON_FALSE,
	JSON_TRUE,
	JSON_NUMBER,
	JSON_STRING,
	JSON_OBJECT,
	JSON_ARRAY
} JSONValueType;

struct JSONValue {
	JSONValueType type;
	union {
		double number;
		JSONString string;
		JSONArray array;
		JSONObject object;
	} val;
};


typedef struct {
	char error[64];
	bool is_text_copied; // if this is true, then json_free will call free on text
	const char *text;
	// root = values[0]
	JSONValue *values;
} JSON;


void process_message(LSP *lsp, JSON *json);
void write_request(LSP *lsp, LSPRequest *request);
void write_message(LSP *lsp, LSPMessage *message);
void lsp_request_free(LSPRequest *r);
void lsp_response_free(LSPResponse *r);

const char *json_type_to_str(JSONValueType type);
void json_debug_print_array(const JSON *json, JSONArray array);
void json_debug_print_object(const JSON *json, JSONObject obj);
void json_debug_print_string(const JSON *json, JSONString string);
void json_debug_print_value(const JSON *json, JSONValue value);
void json_free(JSON *json);
/// NOTE: text must live as long as json!!!
bool json_parse(JSON *json, const char *text);
/// like json_parse, but a copy of text is made, so you can free/overwrite it immediately.
bool json_parse_copy(JSON *json, const char *text);
JSONValue json_object_get(const JSON *json, JSONObject object, const char *name);
JSONValue json_array_get(const JSON *json, JSONArray array, u64 i);
JSONValue json_object_key(const JSON *json, JSONObject object, u64 i);
JSONValue json_object_value(const JSON *json, JSONObject object, u64 i);
double json_force_number(JSONValue x);
double json_object_get_number(const JSON *json, JSONObject object, const char *name);
double json_array_get_number(const JSON *json, JSONArray array, size_t i);
bool json_force_bool(JSONValue x, bool default_value);
bool json_object_get_bool(const JSON *json, JSONObject object, const char *name, bool default_value);
bool json_array_get_bool(const JSON *json, JSONArray array, size_t i, bool default_value);
JSONString json_force_string(JSONValue x);
JSONString json_object_get_string(const JSON *json, JSONObject object, const char *name);
JSONString json_array_get_string(const JSON *json, JSONArray array, size_t i);
JSONObject json_force_object(JSONValue x);
JSONObject json_object_get_object(const JSON *json, JSONObject object, const char *name);
JSONObject json_array_get_object(const JSON *json, JSONArray array, size_t i);
JSONArray json_force_array(JSONValue x);
JSONArray json_object_get_array(const JSON *json, JSONObject object, const char *name);
JSONArray json_array_get_array(const JSON *json, JSONArray array, size_t i);
JSONValue json_root(const JSON *json);
JSONValue json_get(const JSON *json, const char *path);
bool json_has(const JSON *json, const char *path);
void json_string_get(const JSON *json, JSONString string, char *buf, size_t buf_sz);
/// returns a malloc'd null-terminated string.
char *json_string_get_alloc(const JSON *json, JSONString string);
void json_debug_print(const JSON *json);
size_t json_escape_to(char *out, size_t out_sz, const char *in);
char *json_escape(const char *str);
LSPString lsp_response_add_json_string(LSPResponse *response, const JSON *json, JSONString string);
LSPString lsp_request_add_json_string(LSPRequest *request, const JSON *json, JSONString string);
/// free resources used by lsp-write.c
void lsp_write_quit(void);

/// print server-to-client communication
#define LSP_SHOW_S2C 1
/// print client-to-server communication
#define LSP_SHOW_C2S 1

#endif // LSP_INTERNAL

