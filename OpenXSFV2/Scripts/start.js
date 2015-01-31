///<reference path="index.html" />

//<SYS-REG-START>
var scripts = new Array();
progStart(function () {
    for (var i = 0; i < scripts.length; i++) {
        scripts[i](i);
    }
});
//Register startup scripts here using scripts.push()
//Deregister a script using scripts.splice(idx,1);
//<SYS-REG-END>