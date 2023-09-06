/*********************************************************
/* DHS Hamburger Menu class for dynamic page loading
/* Functions:
/*  constructor(Hompage,MenuId): 
/*
/*********************************************************/

class DHSHMenu { // DHS Hamburger Menu class for dynamic page loading

	DHSV="?1.2"; // change version to load new files without caching

  DHSCurrentPage = "";
	DHSHomePage = "";
	DHSSkript = false;
	DHSPop = false;
	
  DHSRefresh() {
    if (this.DHSCurrentPage=="") return; // page not loaded yet, no refresh
		else {
		  var x = this.DHSCurrentPage;
			this.DHSCurrentPage = "";
			this.DHSLoad(x);
		}
	}
		
  DHSLoad(html) { 
    var xhttp = new XMLHttpRequest(); // load page with HTTP request
    var pc = this;

	  // close Hamburger menu
    var myMenu = document.querySelector("#DHSmenu-toggle");
    myMenu.checked = false;

		if (this.DHSCurrentPage == html) return; // already on page, do nothing
		if (this.DHSPageChange() === false) return; // if change not allowed, stay on page
		
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4) {
       if (this.status == 200) { // if HTTP request is successful

        // html in seite einfügen
        document.getElementById("DHSMain").innerHTML=this.response;
				 
				// clear / select menu tab
        if (pc.DHSCurrentPage) document.getElementById(pc.DHSCurrentPage).classList.remove("DHSListSelected");
				pc.DHSCurrentPage = html;
        document.getElementById(html).classList.add("DHSListSelected");
				 
			  var PageSkript = document.createElement('script');
        PageSkript.type = 'text/javascript';
        PageSkript.src = html+'.js'+pc.DHSV;
				PageSkript.charset="UTF-8";
        // Init starten sobald Skript geladen
        PageSkript.onload = pc.DHSScriptLoaded;
        // in den <head> Bereich einfügen			 
	      document.getElementsByTagName('head')[0].appendChild(PageSkript);
 			  pc.DHSSkript = html+'.js'+pc.DHSV;
        // this will add a new entry for the browser history like it would happen if you
        // click on a hyperlink on the page with additional information we can use as needed
        if (!pc.DHSPop) history.pushState(html, null, "?"+html);
				else pc.DHSPop=false;
				document.title = html;
				DHSPageTitle.innerHTML = html;
       }
       else { // if HTTP request not successful, stay on current page
			  console.log("Error, Page "+html+".html not found!");
			 }
			}

    }
		
		xhttp.open("GET",html+".html"+this.DHSV, true);
    xhttp.overrideMimeType('text/plain');
    xhttp.send();
  }

  DHSScriptLoaded() {
    if (typeof DHSStart !== 'undefined') {
		  DHSStart();
		}
  }
	
	DHSPageChange() {
	  if (this.DHSSkript) {
			if (typeof DHSStop !== 'undefined') if (DHSStop() === false) return false; // if DHSStop gives false, stay on page
      var e=document.getElementsByTagName("script");
			for (var i=e.length; i>=0; i--){ //search backwards within nodelist for matching elements to remove
        if (e[i] && e[i].getAttribute("src")!=null && e[i].getAttribute("src").indexOf(this.DHSSkript)!=-1) {
          e[i].parentNode.removeChild(e[i]); //remove element by calling parentNode.removeChild()
					break;
				}
      }
			if (typeof DHSStop !== 'undefined') DHSStop=undefined;
		  if (typeof DHSStart !== 'undefined') DHSStart=undefined;
		  this.DHSSkript = false;
		}
		return true;
	}
	
  constructor(HomePage) {
    var pc = this;
		this.DHSHomePage = HomePage;
		
		window.addEventListener('popstate', function(event) {
    if (event.state !== null && event.state !== undefined) {
		  pc.DHSPop = true; // page already in history
      pc.DHSLoad(event.state);
    }
    });
	
	  this.DHSCurrentPage="";
		var pos=window.location.href.indexOf("?");

    var html = "";
  	if (pos > 0) var html = window.location.href.substring(pos+1);
		if (html ==="") html = HomePage;
			
    this.DHSPop = true; // page already in history
		history.replaceState(html, null, "?"+html);
		this.DHSLoad(html);
	}
}