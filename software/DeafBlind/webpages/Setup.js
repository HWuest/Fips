function DHSStart() {
	Socket.setCallback("s",get_slider,"s");
	Socket.setCallback("d",get_slider,"d");
	Socket.setCallback("a",get_slider,"a");
	Socket.setCallback("x",get_slider,"x");
	Socket.setCallback("m",get_data,"m");
//	Socket.setCallback("M",get_data,"M");
	Socket.setCallback("l",get_check,"l");
	Socket.setCallback("L",get_check,"L");
	Socket.setCallback("t",get_t,"t");
	Socket.setCallback("i",get_w,"i");
	Socket.setCallback("I",get_check,"I");
	Socket.setCallback("w",get_w,"w");
	Socket.send("get s d x I i t l L m a"); // get current field data ( !=19 chars, used for mac compare, to be changed)
	Socket.send("get w");
}

function DHSStop() {
}

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function send_all(data) {
  for (var i = 0; i < data.length; i++) {
	  Socket.send(data[i]); // send char to all
		await delay(document.getElementById('x').value); // wait outspeed time
	}
}

function get_check(data,extra_data) { // get tickMode data
  document.getElementById(extra_data).checked = data>0; // set checkbox
}

function send_check(extra_data) {
	var x=0;
	if (document.getElementById(extra_data).checked) x=1;
	Socket.send(extra_data+" "+x);
}

function get_t(data,extra_data) { // get tickMode data
  document.getElementById("t1").checked=data&1; // set checkbox
  document.getElementById("t2").checked=data&2; // set checkbox
}

function send_t() {
	var x=0;
	if (document.getElementById('t1').checked) x+=1;
	if (document.getElementById('t2').checked) x+=2;
	Socket.send("t "+x);
}

function get_data(data,extra_data) { // get websocket data
  document.getElementById(extra_data).value=data; // set number field
}

function get_slider(data,extra_data) { // get websocket data
  document.getElementById(extra_data).value=data; // set number field
  document.getElementById(extra_data+"s").value=data; // set slider field
}
	
function get_w(data,extra_data) { // get websocket data
  document.getElementById(extra_data).value=data.replace(/~/g,' '); // set wlan data
}

function update() {
    var upload_path = "/fwupdate/";
    var fileInput = document.getElementById("newfile").files;

    /* Max size of an individual file. Make sure this
     * value is same as that set in file_server.c */
    var MAX_FILE_SIZE = 2000*1024;
    var MAX_FILE_SIZE_STR = "2MB";

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else {
      document.getElementById("newfile").disabled = true;
      document.getElementById("update").disabled = true;
 
      if (fileInput[0].size > 2000*1024) {
          alert("File size must be less than 2MB!");
      } else {
          var file = fileInput[0];
          var xhttp = new XMLHttpRequest();
          xhttp.onreadystatechange = function() {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    location.reload();
                } else if (xhttp.status == 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload();
                } else {
                    alert(xhttp.status + " Error!\n" + xhttp.responseText);
                    location.reload();
                }
            }
        };
        xhttp.open("POST", upload_path+file.name, true);
        xhttp.send(file);
        console.log(file.name);
      }
    }
}
