module fetch

go 1.25.0

require dbsync v0.0.0

require (
	github.com/gorilla/websocket v1.5.3 // indirect
	github.com/mattn/go-sqlite3 v1.14.33 // indirect
)

replace dbsync => ../../dbsync
