///<reference path="../index.html" />
progStart(function () {
    progList = new Array();
    var display = XSF.makeOutput();
    display.append('<h1>Establishing connection</h1><hr />Connecting to server. Please wait....');
    var key = Math.random().toString();
    $.post('/setx', key).done(function () {
        var socket = new Socket(window.location.href.replace('http://', 'ws://'), function (state) {
            if (state) {
                //Connection established
                socket.send(key.toArrayBuffer());
                display.remove();
                display = XSF.makeOutput();
                display.html('<h1>Connection with XSF Host Service established</h1><hr /><h2>Welcome to GlobalGrid Web Administration console</h2><hr />');
                display.append('<h2>Installed programs</h2><hr />');
                XSF.loadProgram('Scripts/start.js', function () {
                    for (var i = 0; i < progList.length; i++) {
                        var btn = $(document.createElement('input'));
                        btn.attr('type', 'button');
                        btn.val(progList.toString());
                        display.append(btn);
                    }
                });
                
            } else {
                socket.close();
                alert('Connection to host lost. Please re-start server and/or reload page.');
            }
        }, function (data) {

        });
    });


    
});