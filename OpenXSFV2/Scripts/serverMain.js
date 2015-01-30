///<reference path="../index.html" />
progStart(function () {
    var display = XSF.makeOutput();
    display.append('<h1>Establishing connection</h1><hr />Connecting to server. Please wait....');
    var key = Math.random().toString();
    $.post('/setx', key).done(function () {
        var socket = new Socket(window.location.href.replace('http://', 'ws://'), function (state) {
            if (state) {
                //Connection established
                socket.send(key.toArrayBuffer())
            } else {
                socket.close();
                alert('Connection to host lost. Please re-start server and/or reload page.');
            }
        }, function (data) {

        });
    });


    
});