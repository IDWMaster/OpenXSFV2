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
    var isUTF16 = false;
    for (var i = 0; i < mview.length; i++) {
        if ((mview[i] >> 7) >> 0) {
            //UTF-16 encoding
            isUTF16 = true;
            break;
        }
    }
    if (isUTF16) {
        mview = new Uint16Array(this);
    }
    var retval = String.fromCharCode.apply(null, mview);
    return retval;
};
//Since JavaScript is getting rid of the unescape function,
//there is no way of efficiently encoding UTF-8 strings anymore :(
String.prototype.deprecatedUTF8 = function() {
    return unescape(encodeURIComponent(this));
}
String.prototype.isDiverse = function () {
    for (var i = 0; i < this.length; i++) {
        if ((this.charCodeAt(i) >> 7) > 0) {
            return true;
        }
    }
    return false;
};
String.prototype.china = '嗨';
String.prototype.toArrayBuffer = function () {
    
    var str = this; // TODO: Make this REALLY UTF-8!
    if (str.isDiverse()) {
        //Encode as UTF-16
        var mbuffer = new Uint16Array(this.length);
        for (var i = 0; i < this.length; i++) {
            mbuffer[i] = this.charCodeAt(i);
        }
        return mbuffer.buffer;
    } else {
        //Encode as ASCII (only European characters here)
        var mbuffer = new Uint8Array(this.length);
        for (var i = 0; i < this.length; i++) {
            mbuffer[i] = this.charCodeAt(i);
        }
        return mbuffer.buffer;

    }
};