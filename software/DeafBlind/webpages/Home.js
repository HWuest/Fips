var synth;
var lineLength = 0;
var comMode = 0;
var word = '';

function DHSStart() {
	Socket.setCallback("tick",mesh_data);
	 synth = window.speechSynthesis;
//  window.addEventListener("DOMContentLoaded", () => {	// init speachrecognizer

//  const button = document.getElementById("button");

  SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition
  if (typeof SpeechRecognition === "undefined") {
//    button.remove();
  } else {
    recognition = new SpeechRecognition();
//		recognition.start();
//    recognition.stop();

          rec_stop = () => {
            recognition.stop();
          };

          rec_start = () => {
            recognition.start();
          };
          const onResult = event => {
					  var r= event.results[event.results.length-1][0].transcript;
            console.log("Recognizer: "+r);
						r=replaceUmlaute(r);
						Socket.send(" ");
						Socket.send("o "+r+" ");
          };
    recognition.continuous = false;
    recognition.interimResults = false;
		recognition.addEventListener("result", onResult);
	}
//});

}

function DHSStop() {
}
		
function mesh_data(data,extra_data) {
  var r=document.getElementById('r');
	
	switch (data) {
	  case "ae": data ="&auml;"; break;
	  case "oe": data ="&ouml;"; break;
	  case "ue": data ="&uuml;"; break;
	  case "AE": data ="&Auml;"; break;
	  case "OE": data ="&Ouml;"; break;
	  case "UE": data ="&Uuml;"; break;
	}
	if (comMode === 2) { // comMode active ?
		if (data === 'n') { // newline command ?
			if (!document.getElementById('check2').checked) {
			  r.innerHTML+=data;
				fips(data);
			  lineLength++;
			}
			data='\n'; // set newline char
		}
		if (data === 'z') { // space command ?
			if (!document.getElementById('check2').checked) {
			  r.innerHTML+=data;
				fips(data);
				lineLength++;
			}
			data=' '; // set space char
		}
		if (data === 'l') { // delete char
	    if (document.getElementById('check2').checked) {
		    if (lineLength > 0) {
		      r.innerHTML=r.innerHTML.substring(0,r.innerHTML.length-1); // remove chars
			    lineLength--;
				  fips(data);
		    }
			  return;
		  }
	  }
	  comMode=0; // reset comMode
	}

	if (data==='&ouml;') { // command char ?
		comMode++; // increase comMode
		if (document.getElementById('check2').checked) return;
	}
	else if (comMode) comMode = 0; // reset comMode if not 2 times command char
 
	if (data==='#') { // ccharset char ?
		if (document.getElementById('check2').checked) return;
	} 

	r.innerHTML+=data;
	lineLength ++;
	fips(data);
	
	if (data === '\n') {
	  lineLength = 0;
	}	
	
	if (lineLength==1) {
		r.scrollTop = r.scrollHeight+10;
	}
	
	if (document.getElementById('check3').checked) { // speak word
	  if ((data=== ' ') || (data ==='\n')) {
		  Speak(word);
			word = '';
		}
		else if ((data !== '&ouml;') && (data !=='#')) word += data;
	}

if (lineLength > 20) {
	  lineLength = 0;
		r.innerHTML+='\n';
	}
}

function mesh_enter(event) {
  var x = document.getElementById("s");
  if (event.keyCode == 13) {
		if (x.value.length) {
		  Socket.send(x.value); // send line
		}
		x.value = ""; // clear line
  }
}

function mesh_send() {
  if (document.getElementById('check1').checked) {
	  var x = document.getElementById("s");
    Socket.send(x.value); // send tick
	  x.value = ""; // clear line
	}
}

function Speak(t) {
  if (synth.speaking) {
    console.log("speechSynthesis still speaking");
  }
	
  if (t !== "") {
    const utterThis = new SpeechSynthesisUtterance(t);

//    utterThis.pitch = pitch.value;
//    utterThis.rate = rate.value;
    synth.speak(utterThis);
  }
}
var f_to_char = new Map();
f_to_char.set("a","	D -- -- -- --");
f_to_char.set("b","	-- Z -- -- K");
f_to_char.set("c","	D -- M R K");
f_to_char.set("d","	D -- M -- --");
f_to_char.set("e","	-- Z -- -- --");	
f_to_char.set("f","	D Z -- R --");
f_to_char.set("g","	D -- M R --");
f_to_char.set("h","	-- Z M R K");
f_to_char.set("i","	-- -- M -- --");	
f_to_char.set("j","	D Z M -- K");
f_to_char.set("k","	D -- -- R --");
f_to_char.set("l","	-- -- M R --");
f_to_char.set("m","	-- Z -- R --");
f_to_char.set("n","	D Z -- -- --");
f_to_char.set("o","	-- -- -- R --");
f_to_char.set("p","	D Z -- -- K");
f_to_char.set("q","	-- Z M -- --");
f_to_char.set("r","	D Z M R --");
f_to_char.set("s","	-- -- -- R K");
f_to_char.set("t","	-- Z M -- --");
f_to_char.set("u","	-- -- -- -- K");
f_to_char.set("v","	D Z -- R K");
f_to_char.set("w","	D -- M -- K");
f_to_char.set("x","	-- Z -- R K");
f_to_char.set("y","	D -- -- -- K");
f_to_char.set("z","	-- -- M --  K");
f_to_char.set("&auml;","	D Z M -- --"); 
f_to_char.set("&ouml;"," -- Z M R --"); 
f_to_char.set("&uuml;","	D -- -- R K");
f_to_char.set("*","	-- -- M R K");

function fips(d) {
	var x = document.getElementById("f");
	x.innerHTML = "---";
	var c = f_to_char.get(d);
	if (c !== undefined) f.innerHTML=d+" : "+c;
}

var umlautMap = {
  '\u00dc': 'UE',
  '\u00c4': 'AE',
  '\u00d6': 'OE',
  '\u00fc': 'ue',
  '\u00e4': 'ae',
  '\u00f6': 'oe',
  '\u00df': 'ss',
}

function replaceUmlaute(str) {
  return str
    .replace(/[\u00dc|\u00c4|\u00d6][a-z]/g, (a) => {
      const big = umlautMap[a.slice(0, 1)];
      return big.charAt(0) + big.charAt(1).toLowerCase() + a.slice(1);
    })
    .replace(new RegExp('['+Object.keys(umlautMap).join('|')+']',"g"),
      (a) => umlautMap[a]
    );
}
