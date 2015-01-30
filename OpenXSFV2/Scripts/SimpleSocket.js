Socket = function (url, onConnectionStateChanged, onMessage) {
    ///<summary>A friendlier wrapper for a WebSocket, with built-in retries</summary>
    ///<param name="url">The URL</param>
    ///<param name="onConnectionStateChanged">Triggered on connection state changed. TRUE if connected, FALSE if disconnected.</param>
    ///<param name="onMessage">Occurs when a message is received -- will give you an ArrayBuffer</param>

    var socket = new WebSocket(url);
    var exit = false;

    var retval = {
        close: function () {
            exit = true;
            socket.close();
        },
        send: function (data) {
            ///<summary>Sends a message to the remote server</summary>
            socket.send(data);
        }
    };

    var onConnectionLost = function () {
        if (exit) {
            return;
        }
        onConnectionStateChanged(false);
        socket = new WebSocket(url);
        setupConnection();
    };
    var onConnected = function () {
        if (exit) {
            return;
        }
        onConnectionStateChanged(true);
    };
    var setupConnection = function () {
        socket.onclose = onConnectionLost;
        //socket.onerror = onConnectionLost;
        socket.onopen = onConnected;
        socket.onmessage = function (data) {
            var reader = new FileReader();
            reader.onload = function (buffer) {
                onMessage(this.result);
            };
            reader.readAsArrayBuffer(data.data);
        };
    };
    setupConnection();
    return retval;
};
ArrayBuffer.prototype.toString = function () {
    var mview = new Uint8Array(this);
    var retval = String.fromCharCode.apply(null, mview);
    return retval;
};
String.prototype.toArrayBuffer = function () {
    var utf8 = this; // TODO: Make this REALLY UTF-8!
    var mbuffer = new Uint8Array(utf8.length);
    for (var i = 0; i < utf8.length; i++) {
        mbuffer[i] = utf8.charCodeAt(i);
    }
    return mbuffer.buffer;
};