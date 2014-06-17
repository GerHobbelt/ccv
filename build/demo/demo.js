require.config({
	paths : {
		"facedetection" : "../facedetection"
	}
});

define("demo",["facedetection"],function(detect){

	var container = document.getElementById("image_area");
	var images = ["lincoln.jpg","washington.jpg"];

	function showImage() {

		container.innerHTML = "";

		var i = document.createElement("img"); // <img id="test_image" src="img/lincoln.jpg" width="640" height="480" />
			i.src = "img/" + images[ Math.floor( images.length * Math.random() ) ];
			i.id = "test_image";

		container.appendChild(i);

	}

	document.getElementById("button_detect").onclick = function() {

 		var start = new Date().getTime();

		var results = detect( document.getElementById("test_image"), {
			start : function(img) {
			},
			complete : function(img, coords) {
			},
			error : function(img, code, message) {
			}
		});
		
		var duration = (new Date().getTime() - start) / 1000;

		console.log( duration + " seconds to complete");

		for(var i = 0; i < results.length; i++) {

			var current = results[i];

			var d = document.createElement("div");
				d.className = "face";
				d.style.width = current.width + "px";
				d.style.height = current.height + "px";
				d.style.left = current.x + "px";
				d.style.top = current.y + "px";

			container.appendChild(d);

		}

	};

	document.getElementById("button_reset").onclick = showImage;


	// pick random image to start
	showImage();

});