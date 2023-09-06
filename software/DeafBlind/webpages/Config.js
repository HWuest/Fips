function DHSStart() {
}

function DHSStop() {
}

function upload() {
    var upload_path = "/upload/";
    var fileInput = document.getElementById("newfile").files;

    /* Max size of an individual file. Make sure this
     * value is same as that set in file_server.c */
    var MAX_FILE_SIZE = 2000*1024;
    var MAX_FILE_SIZE_STR = "2MB";

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else {
      document.getElementById("newfile").disabled = true;
      document.getElementById("upload").disabled = true;
      for (var n=0;n < fileInput.length; n++) {
        if (fileInput[0].size > 2000*1024) {
          alert("File size must be less than 2MB!");
        } else {
          var file = fileInput[n];
          var xhttp = new XMLHttpRequest();
          xhttp.onreadystatechange = function() {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    location.reload();
                } else if (xhttp.status == 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload()
                } else {
                    alert(xhttp.status + " Error!\n" + xhttp.responseText);
                    location.reload()
                }
            }
					}
        };
        xhttp.open("POST", upload_path+file.name, true);
        xhttp.send(file);
        console.log(file.name);
      }
    }
}

function fwupload() {
    var upload_path = "/fwupdate/";
    var fileInput = document.getElementById("fwfile").files;

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

function restart() {
	window.location="reset";
}