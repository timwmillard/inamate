package main

/*
#include <stdlib.h>

#ifndef GO_HTTP_HEADER_DEFINED
#define GO_HTTP_HEADER_DEFINED
typedef struct {
    char *key;
    char *value;
} GoHttpHeader;
#endif

typedef struct {
    int status_code;
    unsigned char *body;
    size_t body_len;
    GoHttpHeader *headers;
    char *error;
} GoFetchResponse;
*/
import "C"
import (
	"bytes"
	"io"
	"net/http"
	"sync"
	"unsafe"
)

var (
	fetchMu       sync.Mutex
	fetchInflight = make(map[int]C.GoFetchResponse)
	fetchNextID   int
)

// doFetch performs the HTTP request and returns a GoFetchResponse by value.
// All parameters are pure Go types so this is safe to call from a goroutine.
func doFetch(method, url string, headers http.Header, body []byte) C.GoFetchResponse {
	var result C.GoFetchResponse

	var bodyReader io.Reader
	if len(body) > 0 {
		bodyReader = bytes.NewReader(body)
	}

	req, err := http.NewRequest(method, url, bodyReader)
	if err != nil {
		result.error = C.CString(err.Error())
		return result
	}

	req.Header = headers

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		result.error = C.CString(err.Error())
		return result
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		result.status_code = C.int(resp.StatusCode)
		result.error = C.CString(err.Error())
		return result
	}

	// Collect response headers into NULL-terminated array
	var headerCount int
	for _, vals := range resp.Header {
		headerCount += len(vals)
	}

	if headerCount > 0 {
		// Allocate headerCount + 1 for the NULL sentinel
		cHeaders := (*C.GoHttpHeader)(C.malloc(C.size_t(headerCount+1) * C.size_t(unsafe.Sizeof(C.GoHttpHeader{}))))
		hdrs := unsafe.Slice(cHeaders, headerCount+1)
		i := 0
		for key, vals := range resp.Header {
			for _, val := range vals {
				hdrs[i].key = C.CString(key)
				hdrs[i].value = C.CString(val)
				i++
			}
		}
		// NULL sentinel
		hdrs[i].key = nil
		hdrs[i].value = nil
		result.headers = cHeaders
	}

	result.status_code = C.int(resp.StatusCode)
	result.body = (*C.uchar)(C.CBytes(respBody))
	result.body_len = C.size_t(len(respBody))

	return result
}

// copyHeaders converts a NULL-terminated C header array to http.Header.
func copyHeaders(reqHeaders *C.GoHttpHeader) http.Header {
	h := make(http.Header)
	if reqHeaders == nil {
		return h
	}
	for p := reqHeaders; p.key != nil; p = (*C.GoHttpHeader)(unsafe.Add(unsafe.Pointer(p), unsafe.Sizeof(*p))) {
		h.Set(C.GoString(p.key), C.GoString(p.value))
	}
	return h
}

//export GoFetch
func GoFetch(method, url *C.char, reqHeaders *C.GoHttpHeader, reqBody *C.uchar, reqBodyLen C.size_t) C.GoFetchResponse {
	goMethod := C.GoString(method)
	goURL := C.GoString(url)
	headers := copyHeaders(reqHeaders)
	var body []byte
	if reqBody != nil && reqBodyLen > 0 {
		body = C.GoBytes(unsafe.Pointer(reqBody), C.int(reqBodyLen))
	}

	return doFetch(goMethod, goURL, headers, body)
}

//export GoFetchAsync
func GoFetchAsync(method, url *C.char, reqHeaders *C.GoHttpHeader, reqBody *C.uchar, reqBodyLen C.size_t) C.int {
	// Copy all C data into Go memory before launching the goroutine
	goMethod := C.GoString(method)
	goURL := C.GoString(url)
	headers := copyHeaders(reqHeaders)
	var body []byte
	if reqBody != nil && reqBodyLen > 0 {
		body = C.GoBytes(unsafe.Pointer(reqBody), C.int(reqBodyLen))
	}

	fetchMu.Lock()
	id := fetchNextID
	fetchNextID++
	fetchMu.Unlock()

	go func() {
		resp := doFetch(goMethod, goURL, headers, body)
		fetchMu.Lock()
		fetchInflight[id] = resp
		fetchMu.Unlock()
	}()

	return C.int(id)
}

//export GoFetchPoll
func GoFetchPoll(id C.int, out *C.GoFetchResponse) C.int {
	fetchMu.Lock()
	defer fetchMu.Unlock()
	if resp, ok := fetchInflight[int(id)]; ok {
		delete(fetchInflight, int(id))
		*out = resp
		return 1
	}
	return 0
}

//export GoFetchResponseFree
func GoFetchResponseFree(r *C.GoFetchResponse) {
	if r == nil {
		return
	}
	if r.body != nil {
		C.free(unsafe.Pointer(r.body))
		r.body = nil
		r.body_len = 0
	}
	if r.error != nil {
		C.free(unsafe.Pointer(r.error))
		r.error = nil
	}
	if r.headers != nil {
		for p := r.headers; p.key != nil; p = (*C.GoHttpHeader)(unsafe.Add(unsafe.Pointer(p), unsafe.Sizeof(*p))) {
			C.free(unsafe.Pointer(p.key))
			C.free(unsafe.Pointer(p.value))
		}
		C.free(unsafe.Pointer(r.headers))
		r.headers = nil
	}
}
