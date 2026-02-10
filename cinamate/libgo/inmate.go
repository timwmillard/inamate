package main

/*
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_FWD_DECL_
#define ARENA_FWD_DECL_
typedef struct Arena Arena;
extern void *arena_alloc(Arena *a, size_t size_bytes);
#endif

typedef struct {
    char user_id[64];
    char display_name[64];
    float cursor_x;
    float cursor_y;
    int has_cursor;
    char color[16];
} InPresence;

typedef struct {
    InPresence *entries;
    int count;
} InPresenceList;
*/
import "C"
import (
	"context"
	"crypto/rand"
	"encoding/json"
	"fmt"
	"sync"
	"unsafe"

	"github.com/coder/websocket"
	"github.com/inamate/inamate/backend-go/collab"
	"github.com/inamate/inamate/backend-go/document"
	"github.com/inamate/inamate/backend-go/engine"
)

var (
	eng   *engine.Engine
	engMu sync.Mutex // protects eng across goroutines (C main thread + ws read loop)

	selectedID string
	selectedMu sync.Mutex
)

// --- Presence ---

type presenceInfo struct {
	userID      string
	displayName string
	cursorX     float64
	cursorY     float64
	hasCursor   bool
	color       string
}

var (
	presencesMu  sync.Mutex
	presencesMap = make(map[string]*presenceInfo)
	localUserID  string
)

var presenceColors = []string{
	"#e94560", "#0f3460", "#53d769", "#f5a623",
	"#bd10e0", "#4a90d9", "#50e3c2", "#d0021b",
}

func userIDToColor(userID string) string {
	var hash int32
	for _, c := range userID {
		hash = hash*31 + int32(c)
	}
	if hash < 0 {
		hash = -hash
	}
	return presenceColors[int(hash)%len(presenceColors)]
}

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
	projectID string
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

	ws.projectID = goProjectID

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

	presencesMu.Lock()
	presencesMap = make(map[string]*presenceInfo)
	localUserID = ""
	presencesMu.Unlock()

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
			var payload struct {
				UserID      string `json:"userId"`
				DisplayName string `json:"displayName"`
			}
			if err := json.Unmarshal(msg.Payload, &payload); err == nil {
				presencesMu.Lock()
				localUserID = payload.UserID
				presencesMu.Unlock()
			}
			fmt.Println("ws: welcome received, userId:", payload.UserID)

		case collab.TypePresenceState:
			var payload collab.PresenceStatePayload
			if err := json.Unmarshal(msg.Payload, &payload); err != nil {
				fmt.Println("ws: parse presence.state error:", err)
				continue
			}
			presencesMu.Lock()
			presencesMap = make(map[string]*presenceInfo)
			for uid, p := range payload.Presences {
				entry := &presenceInfo{
					userID:      uid,
					displayName: p.DisplayName,
					color:       userIDToColor(uid),
				}
				if p.Cursor != nil {
					entry.cursorX = p.Cursor.X
					entry.cursorY = p.Cursor.Y
					entry.hasCursor = true
				}
				presencesMap[uid] = entry
			}
			presencesMu.Unlock()

		case collab.TypePresenceUpdate:
			userID := msg.UserID
			if userID == "" {
				continue
			}
			var payload collab.PresencePayload
			if err := json.Unmarshal(msg.Payload, &payload); err != nil {
				fmt.Println("ws: parse presence.update error:", err)
				continue
			}
			presencesMu.Lock()
			entry, ok := presencesMap[userID]
			if !ok {
				entry = &presenceInfo{
					userID: userID,
					color:  userIDToColor(userID),
				}
				presencesMap[userID] = entry
			}
			if payload.DisplayName != "" {
				entry.displayName = payload.DisplayName
			}
			if payload.Cursor != nil {
				entry.cursorX = payload.Cursor.X
				entry.cursorY = payload.Cursor.Y
				entry.hasCursor = true
			} else {
				entry.hasCursor = false
			}
			presencesMu.Unlock()

		case collab.TypePresenceJoin:
			var payload collab.PresenceJoinPayload
			if err := json.Unmarshal(msg.Payload, &payload); err != nil {
				fmt.Println("ws: parse presence.join error:", err)
				continue
			}
			presencesMu.Lock()
			presencesMap[payload.UserID] = &presenceInfo{
				userID:      payload.UserID,
				displayName: payload.DisplayName,
				color:       userIDToColor(payload.UserID),
			}
			presencesMu.Unlock()
			fmt.Println("ws: user joined:", payload.DisplayName)

		case collab.TypePresenceLeave:
			var payload collab.PresenceLeavePayload
			if err := json.Unmarshal(msg.Payload, &payload); err != nil {
				fmt.Println("ws: parse presence.leave error:", err)
				continue
			}
			presencesMu.Lock()
			delete(presencesMap, payload.UserID)
			presencesMu.Unlock()
			fmt.Println("ws: user left:", payload.UserID)
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

// --- Scene Update API ---

//export GoInamateSceneUpdate
func GoInamateSceneUpdate(sceneID, changesJSON *C.char) C.int {
	goSceneID := C.GoString(sceneID)
	goChanges := C.GoString(changesJSON)

	ws.mu.Lock()
	ds := ws.docState
	connected := ws.connected
	conn := ws.conn
	projectID := ws.projectID
	ws.mu.Unlock()

	if ds == nil {
		fmt.Println("scene.update: no document state")
		return -1
	}

	op := collab.Operation{
		Type:    "scene.update",
		SceneID: goSceneID,
		Changes: json.RawMessage(goChanges),
	}

	if _, err := ds.ApplyOperation(op); err != nil {
		fmt.Println("scene.update: apply error:", err)
		return -1
	}

	// Feed updated document back to engine
	docJSON, err := json.Marshal(ds.GetDocument())
	if err != nil {
		fmt.Println("scene.update: marshal error:", err)
		return -1
	}

	engMu.Lock()
	if err := eng.UpdateDocument(string(docJSON)); err != nil {
		fmt.Println("scene.update: engine update error:", err)
	}
	engMu.Unlock()

	// Send to server via websocket
	if connected && conn != nil {
		payload, err := json.Marshal(op)
		if err == nil {
			msg, err := json.Marshal(collab.Message{
				Type:      collab.TypeOpSubmit,
				ProjectID: projectID,
				Payload:   payload,
			})
			if err == nil {
				if err := conn.Write(ws.ctx, websocket.MessageText, msg); err != nil {
					fmt.Println("scene.update: ws write error:", err)
				}
			}
		}
	}

	return 0
}

// --- Object Creation API ---

func newUUID() string {
	var b [16]byte
	rand.Read(b[:])
	b[6] = (b[6] & 0x0f) | 0x40 // version 4
	b[8] = (b[8] & 0x3f) | 0x80 // variant 10
	return fmt.Sprintf("%08x-%04x-%04x-%04x-%012x",
		b[0:4], b[4:6], b[6:8], b[8:10], b[10:16])
}

//export GoInamateCreateRect
func GoInamateCreateRect(x, y, w, h C.float) C.int {
	ws.mu.Lock()
	ds := ws.docState
	connected := ws.connected
	conn := ws.conn
	projectID := ws.projectID
	ws.mu.Unlock()

	if ds == nil {
		fmt.Println("object.create: no document state")
		return -1
	}

	// Find scene root
	doc := ds.GetDocument()
	if len(doc.Project.Scenes) == 0 {
		fmt.Println("object.create: no scenes")
		return -1
	}
	scene, ok := doc.Scenes[doc.Project.Scenes[0]]
	if !ok {
		fmt.Println("object.create: scene not found")
		return -1
	}
	parentID := scene.Root

	objectID := newUUID()
	gw := float64(w)
	gh := float64(h)

	obj := document.ObjectNode{
		ID:       objectID,
		Type:     document.ObjectTypeShapeRect,
		Parent:   &parentID,
		Children: []string{},
		Transform: document.Transform{
			X: float64(x) + gw/2, Y: float64(y) + gh/2,
			SX: 1, SY: 1, R: 0,
			AX: gw / 2, AY: gh / 2,
			SkewX: 0, SkewY: 0,
		},
		Style: document.Style{
			Fill:        "#4a90d9",
			Stroke:      "#2d5a87",
			StrokeWidth: 2,
			Opacity:     1,
		},
		Visible: true,
		Locked:  false,
		Data:    json.RawMessage(fmt.Sprintf(`{"width":%g,"height":%g}`, gw, gh)),
	}

	objJSON, err := json.Marshal(obj)
	if err != nil {
		fmt.Println("object.create: marshal object error:", err)
		return -1
	}

	op := collab.Operation{
		Type:     "object.create",
		Object:   objJSON,
		ParentID: parentID,
	}

	if _, err := ds.ApplyOperation(op); err != nil {
		fmt.Println("object.create: apply error:", err)
		return -1
	}

	// Feed updated document back to engine
	docJSON, err := json.Marshal(ds.GetDocument())
	if err != nil {
		fmt.Println("object.create: marshal doc error:", err)
		return -1
	}

	engMu.Lock()
	if err := eng.UpdateDocument(string(docJSON)); err != nil {
		fmt.Println("object.create: engine update error:", err)
	}
	engMu.Unlock()

	// Send to server via websocket
	if connected && conn != nil {
		payload, err := json.Marshal(op)
		if err == nil {
			msg, err := json.Marshal(collab.Message{
				Type:      collab.TypeOpSubmit,
				ProjectID: projectID,
				Payload:   payload,
			})
			if err == nil {
				if err := conn.Write(ws.ctx, websocket.MessageText, msg); err != nil {
					fmt.Println("object.create: ws write error:", err)
				}
			}
		}
	}

	return 0
}

//export GoInamateDeleteAll
func GoInamateDeleteAll() C.int {
	ws.mu.Lock()
	ds := ws.docState
	connected := ws.connected
	conn := ws.conn
	projectID := ws.projectID
	ws.mu.Unlock()

	if ds == nil {
		fmt.Println("delete_all: no document state")
		return -1
	}

	doc := ds.GetDocument()
	if len(doc.Project.Scenes) == 0 {
		return 0
	}
	scene, ok := doc.Scenes[doc.Project.Scenes[0]]
	if !ok {
		return 0
	}

	root, ok := doc.Objects[scene.Root]
	if !ok || len(root.Children) == 0 {
		return 0
	}

	// Collect children to delete (snapshot before mutation)
	children := make([]string, len(root.Children))
	copy(children, root.Children)

	for _, childID := range children {
		op := collab.Operation{
			Type:     "object.delete",
			ObjectID: childID,
		}

		if _, err := ds.ApplyOperation(op); err != nil {
			fmt.Println("delete_all: apply error:", err)
			continue
		}

		// Send to server
		if connected && conn != nil {
			payload, err := json.Marshal(op)
			if err == nil {
				msg, err := json.Marshal(collab.Message{
					Type:      collab.TypeOpSubmit,
					ProjectID: projectID,
					Payload:   payload,
				})
				if err == nil {
					if err := conn.Write(ws.ctx, websocket.MessageText, msg); err != nil {
						fmt.Println("delete_all: ws write error:", err)
					}
				}
			}
		}
	}

	// Feed updated document back to engine
	docJSON, err := json.Marshal(ds.GetDocument())
	if err != nil {
		fmt.Println("delete_all: marshal doc error:", err)
		return -1
	}

	engMu.Lock()
	if err := eng.UpdateDocument(string(docJSON)); err != nil {
		fmt.Println("delete_all: engine update error:", err)
	}
	engMu.Unlock()

	return 0
}

// --- Presence API ---

//export GoInamateSendCursor
func GoInamateSendCursor(x, y C.float) {
	ws.mu.Lock()
	conn := ws.conn
	connected := ws.connected
	projectID := ws.projectID
	ws.mu.Unlock()

	if !connected || conn == nil {
		return
	}

	payload, err := json.Marshal(collab.PresencePayload{
		Cursor:    &collab.CursorPos{X: float64(x), Y: float64(y)},
		Selection: []string{},
	})
	if err != nil {
		return
	}
	msg, err := json.Marshal(collab.Message{
		Type:      collab.TypePresenceUpdate,
		ProjectID: projectID,
		Payload:   payload,
	})
	if err != nil {
		return
	}
	if err := conn.Write(ws.ctx, websocket.MessageText, msg); err != nil {
		fmt.Println("ws: cursor write error:", err)
	}
}

//export GoInamateGetPresences
func GoInamateGetPresences(a *C.Arena) C.InPresenceList {
	presencesMu.Lock()
	localID := localUserID

	entries := make([]*presenceInfo, 0, len(presencesMap))
	for uid, p := range presencesMap {
		if uid != localID {
			entries = append(entries, p)
		}
	}
	presencesMu.Unlock()

	n := len(entries)
	if n == 0 {
		return C.InPresenceList{}
	}

	arr := (*C.InPresence)(C.arena_alloc(a, C.size_t(n)*C.size_t(unsafe.Sizeof(C.InPresence{}))))
	slice := unsafe.Slice(arr, n)

	for i, p := range entries {
		e := &slice[i]
		C.memset(unsafe.Pointer(e), 0, C.size_t(unsafe.Sizeof(C.InPresence{})))
		copyToCharArray(&e.user_id[0], 64, p.userID)
		copyToCharArray(&e.display_name[0], 64, p.displayName)
		e.cursor_x = C.float(p.cursorX)
		e.cursor_y = C.float(p.cursorY)
		if p.hasCursor {
			e.has_cursor = 1
		}
		copyToCharArray(&e.color[0], 16, p.color)
	}

	return C.InPresenceList{
		entries: arr,
		count:   C.int(n),
	}
}

// --- Selection / Hit Test API ---

//export GoInamateHitTest
func GoInamateHitTest(x, y C.float, outID *C.char, outIDLen C.int) C.int {
	engMu.Lock()
	id := eng.HitTest(float64(x), float64(y))
	if id != "" {
		eng.SetSelection([]string{id})
	} else {
		eng.SetSelection(nil)
	}
	engMu.Unlock()

	selectedMu.Lock()
	selectedID = id
	selectedMu.Unlock()

	if outID != nil && outIDLen > 0 {
		copyToCharArray(outID, int(outIDLen), id)
	}

	if id != "" {
		return 1
	}
	return 0
}

//export GoInamateClearSelection
func GoInamateClearSelection() {
	selectedMu.Lock()
	selectedID = ""
	selectedMu.Unlock()

	engMu.Lock()
	eng.SetSelection(nil)
	engMu.Unlock()
}

//export GoInamateGetSelectionBounds
func GoInamateGetSelectionBounds(outX, outY, outW, outH *C.float) C.int {
	engMu.Lock()
	boundsJSON := eng.GetSelectionBounds()
	engMu.Unlock()

	var bounds struct {
		X      float64 `json:"x"`
		Y      float64 `json:"y"`
		Width  float64 `json:"width"`
		Height float64 `json:"height"`
	}
	if err := json.Unmarshal([]byte(boundsJSON), &bounds); err != nil {
		return 0
	}
	if bounds.Width <= 0 || bounds.Height <= 0 {
		return 0
	}

	*outX = C.float(bounds.X)
	*outY = C.float(bounds.Y)
	*outW = C.float(bounds.Width)
	*outH = C.float(bounds.Height)
	return 1
}

// --- Object Update API ---

//export GoInamateObjectTransform
func GoInamateObjectTransform(objectID, changesJSON *C.char) C.int {
	goObjectID := C.GoString(objectID)
	goChanges := C.GoString(changesJSON)

	ws.mu.Lock()
	ds := ws.docState
	connected := ws.connected
	conn := ws.conn
	projectID := ws.projectID
	ws.mu.Unlock()

	if ds == nil {
		fmt.Println("object.transform: no document state")
		return -1
	}

	op := collab.Operation{
		Type:      "object.transform",
		ObjectID:  goObjectID,
		Transform: json.RawMessage(goChanges),
	}

	if _, err := ds.ApplyOperation(op); err != nil {
		fmt.Println("object.transform: apply error:", err)
		return -1
	}

	docJSON, err := json.Marshal(ds.GetDocument())
	if err != nil {
		fmt.Println("object.transform: marshal error:", err)
		return -1
	}

	engMu.Lock()
	if err := eng.UpdateDocument(string(docJSON)); err != nil {
		fmt.Println("object.transform: engine update error:", err)
	}
	engMu.Unlock()

	if connected && conn != nil {
		payload, err := json.Marshal(op)
		if err == nil {
			msg, err := json.Marshal(collab.Message{
				Type:      collab.TypeOpSubmit,
				ProjectID: projectID,
				Payload:   payload,
			})
			if err == nil {
				if err := conn.Write(ws.ctx, websocket.MessageText, msg); err != nil {
					fmt.Println("object.transform: ws write error:", err)
				}
			}
		}
	}

	return 0
}

//export GoInamateObjectStyle
func GoInamateObjectStyle(objectID, changesJSON *C.char) C.int {
	goObjectID := C.GoString(objectID)
	goChanges := C.GoString(changesJSON)

	ws.mu.Lock()
	ds := ws.docState
	connected := ws.connected
	conn := ws.conn
	projectID := ws.projectID
	ws.mu.Unlock()

	if ds == nil {
		fmt.Println("object.style: no document state")
		return -1
	}

	op := collab.Operation{
		Type:     "object.style",
		ObjectID: goObjectID,
		Style:    json.RawMessage(goChanges),
	}

	if _, err := ds.ApplyOperation(op); err != nil {
		fmt.Println("object.style: apply error:", err)
		return -1
	}

	docJSON, err := json.Marshal(ds.GetDocument())
	if err != nil {
		fmt.Println("object.style: marshal error:", err)
		return -1
	}

	engMu.Lock()
	if err := eng.UpdateDocument(string(docJSON)); err != nil {
		fmt.Println("object.style: engine update error:", err)
	}
	engMu.Unlock()

	if connected && conn != nil {
		payload, err := json.Marshal(op)
		if err == nil {
			msg, err := json.Marshal(collab.Message{
				Type:      collab.TypeOpSubmit,
				ProjectID: projectID,
				Payload:   payload,
			})
			if err == nil {
				if err := conn.Write(ws.ctx, websocket.MessageText, msg); err != nil {
					fmt.Println("object.style: ws write error:", err)
				}
			}
		}
	}

	return 0
}
