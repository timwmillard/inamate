package main

/*
#include <stdlib.h>
*/
import "C"
import (
	"context"
	"encoding/json"
	"fmt"
	"sync"

	"github.com/coder/websocket"
	"github.com/inamate/inamate/backend-go/collab"
	"github.com/inamate/inamate/backend-go/document"
	"github.com/inamate/inamate/backend-go/engine"
)

var (
	eng   *engine.Engine
	engMu sync.Mutex // protects eng across goroutines (C main thread + ws read loop)
)

// --- Engine ---

//export GoInamateNewEngine
func GoInamateNewEngine() {
	eng = engine.NewEngine()
}

// --- WebSocket / Collab ---

type wsConn struct {
	conn      *websocket.Conn
	ctx       context.Context
	cancel    context.CancelFunc
	mu        sync.Mutex
	connected bool
	docLoaded bool
	docState  *collab.DocumentState
}

var ws wsConn

//export GoInamateConnect
func GoInamateConnect(serverURL, projectID *C.char) C.int {
	return GoInamateConnectAuth(serverURL, projectID, nil)
}

//export GoInamateConnectAuth
func GoInamateConnectAuth(serverURL, projectID, token *C.char) C.int {
	goServerURL := C.GoString(serverURL)
	goProjectID := C.GoString(projectID)

	wsURL := goServerURL + "/ws/project/" + goProjectID
	if len(wsURL) > 5 && wsURL[:5] == "https" {
		wsURL = "wss" + wsURL[5:]
	} else if len(wsURL) > 4 && wsURL[:4] == "http" {
		wsURL = "ws" + wsURL[4:]
	}

	if token != nil {
		wsURL += "?token=" + C.GoString(token)
	}

	ws.ctx, ws.cancel = context.WithCancel(context.Background())

	conn, _, err := websocket.Dial(ws.ctx, wsURL, nil)
	if err != nil {
		fmt.Println("ws: dial error:", err)
		ws.cancel()
		return -1
	}

	ws.conn = conn
	ws.mu.Lock()
	ws.connected = true
	ws.docLoaded = false
	ws.docState = nil
	ws.mu.Unlock()

	fmt.Println("ws: connected to", wsURL)

	go wsReadLoop()

	return 0
}

func wsReadLoop() {
	defer func() {
		ws.mu.Lock()
		ws.connected = false
		ws.mu.Unlock()
		fmt.Println("ws: read loop exited")
	}()

	for {
		_, data, err := ws.conn.Read(ws.ctx)
		if err != nil {
			fmt.Println("ws: read error:", err)
			return
		}

		var msg collab.Message
		if err := json.Unmarshal(data, &msg); err != nil {
			fmt.Println("ws: parse error:", err)
			continue
		}

		switch msg.Type {
		case collab.TypeDocSync:
			// Parse document and create authoritative state
			var doc document.InDocument
			if err := json.Unmarshal(msg.Payload, &doc); err != nil {
				fmt.Println("ws: parse doc error:", err)
				continue
			}

			engMu.Lock()
			if err := eng.LoadDocument(string(msg.Payload)); err != nil {
				fmt.Println("ws: load document error:", err)
				engMu.Unlock()
				continue
			}
			engMu.Unlock()

			ws.mu.Lock()
			ws.docState = collab.NewDocumentState(&doc)
			ws.docLoaded = true
			ws.mu.Unlock()
			fmt.Println("ws: document loaded")

		case collab.TypeOpBroadcast:
			var broadcast collab.OperationBroadcastPayload
			if err := json.Unmarshal(msg.Payload, &broadcast); err != nil {
				fmt.Println("ws: parse op.broadcast error:", err)
				continue
			}

			ws.mu.Lock()
			ds := ws.docState
			ws.mu.Unlock()

			if ds == nil {
				continue
			}

			if _, err := ds.ApplyOperation(broadcast.Operation); err != nil {
				fmt.Println("ws: apply operation error:", err)
				continue
			}

			// Feed updated document back to engine
			docJSON, err := json.Marshal(ds.GetDocument())
			if err != nil {
				fmt.Println("ws: marshal doc error:", err)
				continue
			}

			engMu.Lock()
			if err := eng.UpdateDocument(string(docJSON)); err != nil {
				fmt.Println("ws: update document error:", err)
			}
			engMu.Unlock()

		case collab.TypeWelcome:
			fmt.Println("ws: welcome received")
		}
	}
}

//export GoInamateIsConnected
func GoInamateIsConnected() C.int {
	ws.mu.Lock()
	defer ws.mu.Unlock()
	if ws.connected {
		return 1
	}
	return 0
}

//export GoInamateIsDocLoaded
func GoInamateIsDocLoaded() C.int {
	ws.mu.Lock()
	defer ws.mu.Unlock()
	if ws.docLoaded {
		return 1
	}
	return 0
}

//export GoInamateSend
func GoInamateSend(jsonMsg *C.char) C.int {
	ws.mu.Lock()
	conn := ws.conn
	connected := ws.connected
	ws.mu.Unlock()

	if !connected || conn == nil {
		return -1
	}

	msg := C.GoString(jsonMsg)
	if err := conn.Write(ws.ctx, websocket.MessageText, []byte(msg)); err != nil {
		fmt.Println("ws: write error:", err)
		return -1
	}
	return 0
}

//export GoInamateDisconnect
func GoInamateDisconnect() {
	ws.mu.Lock()
	conn := ws.conn
	ws.connected = false
	ws.docLoaded = false
	ws.docState = nil
	ws.mu.Unlock()

	if conn != nil {
		conn.Close(websocket.StatusNormalClosure, "bye")
	}
	if ws.cancel != nil {
		ws.cancel()
	}
	fmt.Println("ws: disconnected")
}
