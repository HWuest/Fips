/*********************************************************
/* DHS Socket class for ESP WebSocket Data communication
/* Functions:
/*  constructor(optional host): opens WebSoket on current host if parameter not given, otherwise given host
/*  setCallback(command, function, optinal callingclass): sets "command" and function as callback function for WebSocket message recieve
/*    Callback-Function: xyz(data,callingclass) { ... }
/*  clearCallback(): remove all callback functions
/*  send(message): send message over WebSocket channel
/*
/*********************************************************/

class DHSSocket { // DHS WebSocket class for ESP Data-Communication
  
	mySocket; // socket for data connection
  msg = []; // message buffer
	msgfunc = []; // message process functions
	
  constructor(host=window.location.host) {
    this.open(host);
	}
    
  open(host) {
	  // open socket connection to server on same IP as web page
    console.log("DHS open socket on "+host);
		var pc = this;
		
    if (typeof this.mySocket !== 'undefined') this.mySocket.close(); // close old socket
    if (host === "") {
		  host="192.168.178.54"; // fallback for local tests
		  console.log("No host given during local start!, therefore "+host+" used");
		}
		if (window.location.protocol === "https:") this.mySocket = new WebSocket('wss://' + host + '/ws'); // open new https socket connection
		else this.mySocket = new WebSocket('ws://' + host + '/ws'); // open new http socket connection
		
	  this.mySocket.onmessage = function(evt) { // callback function
     var recmsg = evt.data.split(" "); // split up message data
		 var j=0;
		 do {
      for (var c = 0; c < pc.msgfunc.length; c++) { // itterate throug mesage functions
        if (pc.msgfunc[c][0] === recmsg[j]) {

          if (pc.msgfunc[c][2] === "") pc.msgfunc[c][1](recmsg[j+1]); // call callback function with msgdata
					else pc.msgfunc[c][1](recmsg[j+1],pc.msgfunc[c][2]); // call callback function with msgdata and classpointer
			    break;
		    } 
      }
			j += 2;
		 } while (j < recmsg.length);
	  }

    this.mySocket.onopen = function() { // socket opened callback
		  console.log("Done, socket opened");
			
      while (pc.msg.length > 0) {  // send pending messages after socket open 
        pc.mySocket.send(pc.msg[0])
        pc.msg.shift();
      }
    }
		
    this.mySocket.onerror = function (error) {
      console.log('WebSocket Error, try reconnect...');
      pc.open(host); // retry connection
    }
 	}
	
	close() {
	  console.log("DHS close socket");
		this.mySocket.close(); 
	}
	
	setCallback(command,func,extraData="") { // set command callback function.
	  var x = [command, func,extraData];
		this.msgfunc.push(x);
	}

  clear() {
	  this.msgfunc = [];
	}
	
  send(txt) { // send data to server
    if (typeof this.mySocket !== "undefined") { // if socket is opend
        if (this.mySocket.readyState !== 1) { // if socket is not ready
            this.msg.push(txt); // store message in buffer
						if (this.mySocket.readyState==3) {
						  this.close();
						  this.open(window.location.host); // reopen socket
						}
        } else this.mySocket.send(txt) // directly send message to server
    }
    else console.log("Error, Socket was closed!");
  }
}
