# BOSSWAVE Bindings for C

## Overview
This library provides an implementation of the BOSSWAVE Out-of-Band (OOB) protocol in the C programming language. It allows Out-of-Band BOSSWAVE clients written in C to communicate with a local BOSSWAVE agent. It is designed to work with Linux and RIOT applications.

This library can be configured to run _without using malloc_. Rather, the user provides the library with a buffer used as a _frame heap_ that is large enough to hold a single OOB frame. Received frames are stored in this space, and all other memory allocation is done on the stack. Alternatively, if no frame heap is provided, the library falls back to standard dynamic memory allocation using `malloc`. There are several reasons why it may be desirable to avoid using `malloc`. First, if frames are allocated dynamically, a very large frame may consume all of memory. While this is often not a concern for a system running Linux, a RIOT application running on a memory-constrained device using `malloc` for other purposes may crash if a very large frame is received due to some sort of memory. Second, avoiding `malloc` allows one to achieve a higher level of determinism than would be possible otherwise. If a different part of the application is using `malloc`, then there may not be sufficient heap space if the BOSSWAVE bindings are also using `malloc` and a frame arrives at the "wrong" time. Using an appropriately-sized frame heap solves this problem, because it guarantees that enough memory is available to load all frames in which the application is interested.

## Dependencies
The only dependencies for Linux are libc and pthreads.
For RIOT, the dependencies include an implementation of TCP satisfying the `conn` API. Such an implementation is provided in Pull Request \#12 on the hamilton-mote/RIOT-OS repository on GitHub.

## How to Compile this Library
To compile for Linux using gcc, use the `-pthread` and `-DBW_OS=LINUX` flags.
To compile for RIOT using gcc, use the `-DBW2_OS=RIOT` flag. This can be done by adding `CFLAGS += -DBW2_OS=RIOT` to the Makefile.

## Naming Convention
All `#define`'d variables are prefixed with `BW2_`.
All struct names are prefixed with `bw2_` and no struct names are typedef'd.
All function names are prefixed with either `bw2_` or `_bw2_`.

## Support for the Out-of-Band Protocol
Currently, the "old" API (i.e. the functions in https://github.com/immesys/bw2bind/blob/master/api.go) is supported. Support for the "new" API (i.e. the functions in https://github.com/immesys/bw2bind/blob/master/newapi.go) is in the works.

## Concurrency Model
Each BOSSWAVE client, represented in code with `struct bw2_client`, consists of a separate TCP connection to the BOSSWAVE agent and a thread (the "BOSSWAVE thread") that reads received frames from that connection. API calls (see api.h) can be made on any other thread, and will block until the response frame is received.

Some API calls, such as subscribe, query, and list, may return multiple results contained in multiple frames. These calls operate asynchronously: they block until the initial response frame is received, and provide the actual results later. The BOSSWAVE bindings for the Go programming language (found in the immesys/bw2bind repository on GitHub) handle this by returning a channel which is populated by results as they arrive. The approach used by this library is to invoke a function, provided by the user, each time a new result is available. When invoking the API function, the user provides a function pointer as an argument as well as a context blob, and when a new result is available, the function is invoked, with the result and provided context blob provided as arguments. The user-provided function returns a boolean. If it is `false`, the user keeps listening for more results; if it is `true`, additional results are ignored.

The user-provided function is invoked on the BOSSWAVE thread, so it is not advisable to perform any operations in the user-defined function that will block for a long time. Making any API calls within a user-defined function will cause deadlock. Furthermore, because the received frame and any return-value structures (such as `struct bw2_simpleMessage` and `struct bw2_simpleChain`) is stack-allocated in the BOSSWAVE thread, any pointers passed as arguments to a user-provided function, and any pointers within structures passed as arguments to a user-provided function, will not be valid after the user-provided function returns. If the data is needed after the user-provided function returns, the user should make a copy of the needed data.

## The API

### Overview
To use the API, include the appropriate header file:
```
#include "bw2/api.h"
```
In the Go bindings, the API calls take parameters via a struct rather than via function arguments, to allow default values for some parameters. Similarly, many API calls in this library take in parameters via a struct. Clearing a struct with `memset` will set parameters to their default values, just as leaving struct members unspecified in the Go bindings sets them to their default values.

### Error Codes

Upon error, a function may either return -1, in which case, errno should be checked to determine the cause of the error, or a positive number, in which case the return value is one of the BOSSWAVE error codes found in `errors.h`. Upon success, it returns 0.

### Structure and Union Definitions

#### Context Structures

Structure and union definitions can also be found in `api.h`.

```
union bw2_userctx {
    void* ptr;
    int32_t val;
};
```
This union describes the type of the context provided by the user with each user-defined function. It is provided back to the user-provided function whenever it is called.

API calls that return multiple values, such as query, list, and subscribe, use _contexts_ to keep track of the pending requests. As described above, the user provides a function and context, and the function is invoked with the context as the final argument, whenever a new result is available. The function and context are provided as elements of the context structure. Below are some examples:

```
struct bw2_simplemsg_ctx {
    /* The user sets this element. */
    bool (*on_message)(struct bw2_simpleMessage* sm, bool final, int error, union bw2_userctx ctx);
    union bw2_userctx ctx;

    /* The remaining elements are used internally by the bindings. */
    struct bw2_reqctx reqctx;
};

struct bw2_chararr_ctx {
    /* The user sets this element. */
    bool (*on_message)(char* arr, size_t arrlen, bool final, int error, union bw2_userctx ctx);
    union bw2_userctx ctx;

    /* The remaining elements are used internally by the bindings. */
    struct bw2_reqctx reqctx;
};

struct bw2_simplechain_ctx {
    /* The user sets this element. */
    bool (*on_chain)(struct bw2_simpleChain* sc, bool final, int error, union bw2_userctx ctx);
    union bw2_userctx ctx;

    /* The remaining elements are used internally by the bindings. */
    struct bw2_reqctx reqctx;
};
```

The context structure is allocated by the user, and then passed in with the API call. The context structure also contains additional elements used internally by this library, which contain data needed to keep track of the pending request and correctly deliver results as they become available. _Therefore, the user must not deallocate the context structure until all results have been delivered._ The `final` parameter, passed to each user-provided function, when set to `true`, indicates that the function will not be invoked any more times, and that the last result has been delivered and that the context structure can be deallocated.

The user may also stop listening to additional results via the return value of the user-provided function. If the function returns `true`, then the library will not call the function any more, and the context structure can be deallocated. Note that for subscriptions, this is _not_ the same as unsubscribing from the URI! If the function returns `true`, then the agent will continue to deliver frames matching the subscription, but they will just be ignored by the library and will not be delivered to the user. Rather, use `bw2_unsubscribe` to properly unsubscribe from a URI.

Finally, not every invocation of the user-provided function indicates that a new value is available. If the first argument to the function is `NULL`, then no new value is available, but the FINAL and ERROR parameters should still be checked.

#### Parameter Structures

Because the API calls take many parameters, they are often passed via structures instead of raw function parameters. An example is provided below.
```
struct bw2_publishParams {
    char* uri;
    struct bw2_dotChainHash* primaryAccessChain;
    bool autochain;
    struct bw2_routingobj* routingObjects;
    struct bw2_payloadobj* payloadObjects;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* elaboratePAC;
    bool doNotVerify;
    bool persist;
};
```
First, note that if the entire struct is `memset` to `0x00`, then all parameters take on their default values. This allows the user to `memset` the parameter struct to zero, and then set specific parameters to override the defaults.

Second, note that only one of `expiry` and `expirydelta` needs to be set. If both are set, only `expiry` will be used.

Third, `expiry` should be set according to UTC time, not local time.

Fourth, `routingObjects` and `payloadObjects` are linked lists. Each `struct bw2_payloadobj` and `struct bw2_routingobj` has a `next` element which is a pointer to an object of the same type. This `next` pointer is used to chain them together to create linked lists. The pointer for the last element in the list should be set to `NULL`.

Below is the parameter struct for `bw2_createDOT`:
```
struct bw2_createDOTParams {
    bool isPermission;
    struct bw2_vkHash* to;
    uint8_t ttl;
    struct tm* expiry;
    uint64_t expiryDelta;
    char* contact;
    char* comment;
    struct bw2_vkHash* revokers;
    bool omitCreationDate;
    char* uri;
    char* accessPermissions;
};
```
Similar to `struct bw2_payloadobj` and `struct bw2_routingobj`, `struct bw2_vkHash` has a `next` field, allowing to form linked lists. In the parameters to `bw2_createDOT`, `revokers` consists of a linked list of hashes, but `to` is a single hash (not a linked list).

#### Object Structures

Some BOSSWAVE objects have dedicated structures, such as:
```
struct bw2_vk {
    char vk[BW2_OBJECTS_MAX_VK_LENGTH];
    size_t vklen;
};

struct bw2_vkHash {
    char vkhash[BW2_OBJECTS_MAX_VK_HASH_LENGTH];
    size_t vkhashlen;

    /* Useful for specifying lists of entities. */
    struct bw2_vkHash* next;

    /* If added as a parameter to an API call, this is used.
     * It does not need to be set externally by the user.
     */
    struct bw2_header hdr;
};

struct bw2_dot {
    char dot[BW2_OBJECTS_MAX_DOT_LENGTH];
    size_t dotlen;
};
```
There are dedicated functions in `objects.h` used to set these structs, but the `next` pointer can be used to arrange them in linked lists to pass them as parameters to API calls. If needed, the data blobs can be read directly from the structs.

### Payload Object Numbers
Payload object numbers and dot forms can be found in `ponum.h`.

### The Functions

```
int bw2_clientInit(struct bw2_client* client);
```
This function initializes a client structure, setting its members to their initial values. It does not actually create an active BOSSWAVE OOB session, but must be called before calling `bw2_connect`.

```
int bw2_connect(struct bw2_client* client, const struct sockaddr* addr, socklen_t addrlen, char* frameheap, size_t heapsize);
```
This function connects to the specified BOSSWAVE agent, and creates the BOSSWAVE thread for the connection. The provided frame heap is used to store frames that are read from the agent.

```
int bw2_disconnect(struct bw2_client* client);
```
This function causes the client to disconnect from the current BOSSWAVE agent.

```
bool bw2_isConnected(struct bw2_client* client);
```
This function returns a boolean indicating whether the client is currently connected to a BOSSWAVE agent.

```
int bw2_setEntity(struct bw2_client* client, char* entity, size_t entitylen, struct bw2_vkHash* vkhash);
```
This sets the entity used in API calls to the entity described by the `entity` blob (whose length is `entitylen`). The hash of the verifying key, returned by the BOSSWAVE agent, is stored in the `vkhash` parameter, if the provided pointer is not `NULL`.

```
int bw2_publish(struct bw2_client* client, struct bw2_publishParams* p);
```
Publishes or persists a message or messages based on the parameters `p`.

```
int bw2_subscribe(struct bw2_client* client, struct bw2_subscribeParams* p, struct bw2_simplemsg_ctx* subctx, struct bw2_subscriptionHandle* handle);
```
Subscribes to a URI based on the parameters `p`. The subscription context (`subctx`) has an element that is a function pointer, and another element that contains the user context. When a message is received, that function is invoked, with the user context as the final argument.

In addition to storing the user-provided function and context, `subctx` contains fields used internally by this library to keep track of the subscription. _Therefore, the subscription context cannot be deallocated until either (1) the function is called with the parameter named "final" set to `true` (which will happen if the user unsubscribes), or (3) the user returns `true` from the user-defined function to stop listening for subscription results._

If `handle` is not `NULL`, the subscription handle is stored into the structure to which it points. The handle can later be used to unsubscribe from the URI using `bw2_unsubscribe`.

```
int bw2_query(struct bw2_client* client, struct bw2_queryParams* p, struct bw2_simplemsg_ctx* qctx);
```
Queries a URI for persisted messages based on the parameters `p`. The query context (`qctx`) functions exactly like the `subctx` for `bw2_subscribe`: the user sets two fields in `qctx`, namely the user-provided function and context, and must keep `qctx` in memory until the final result is received, or until the user stops listening for responses by returning `true`.

```
int bw2_list(struct bw2_client* client, struct bw2_listParams* p, struct bw2_chararr_ctx* lctx);
```
Lists children of the provided URI that have persisted messages, based on the parameters `p`. The list context (`lctx`) functions exactly like the `subctx` for `bw2_subscribe`: the user sets two fields in `lctx`, namely the user-provided function and context, and must keep `lctx` in memory until the final result is received, or until the user stops listening for responses by returning `true`.

```
int bw2_createDOT(struct bw2_client* client, struct bw2_createDOTParams* p, struct bw2_dotHash* dothash, struct bw2_dot* dot);
```
Grants a Declaration of Trust (DoT) according to the parameters `p`. The `dothash` and `dot` parameters, if not `NULL`, are filled in with the hash and binary representation, respectively, of the granted DoT.

```
int bw2_createEntity(struct bw2_client* client, struct bw2_createEntityParams* p, struct bw2_vkHash* vkhash, struct bw2_vk* vk);
```
Creates a new entity according to the parameters `p`. The `vkhash` and `vk` parameters, if not `NULL`, are filled in with the hash and binary representation, respectively, of the verifying key of the created entity.

```
int bw2_createDOTChain(struct bw2_client* client, struct bw2_createDOTChainParams* p, struct bw2_dotChainHash* dotchainhash);
```
Creates a DoT Chain out of a series of Declarations of Trust (DoTs) according to the parameters `p`. The `dotchainhash` parameter, if not `NULL`, is filled in with the hash of the created DoT Chain.

```
int bw2_buildChain(struct bw2_client* client, struct bw2_buildChainParams* p, struct bw2_simplechain_ctx* scctx);
```
Requests the BOSSWAVE agent to build a DoT chain according to the parameters `p`. The simple chain context `scctx` functions exactly like the subscription context for `bw2_subscribe`: the user sets two elements of the struct to specify a function and context, and must keep `scctx` in memory until all chains have been returned or the user stops listening for responses.

```
int bw2_unsubscribe(struct bw2_client* client, struct bw2_subscriptionHandle* handle);
```
Cancels the subscription corresponding to `handle`. The handle of a subscription is obtained when calling `bw2_subscribe`. The function provided by the user to `bw2_subscribe` will be invoked once more with a NULL message and with the `final` argument set to `true`. One can only unsubscribe from a URI with the same client with which the subscription was made.
