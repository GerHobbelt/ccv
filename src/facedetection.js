/*
 FaceDetection AMD
 Ported from jquery facetection plugin by Jay Salvat
 Copyright (c) 2010, Jay Salvat
 AMD Port (c) 2013 by sprky0

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
define("facedetection", ["ccv/ccv", "ccv/face"], function(ccv, cascade) {

	function facedetection(target, settings) {

		var options = {
			confidence : null,
			start : function(img) {
			},
			complete : function(img, coords) {
			},
			error : function(img, code, message) {
			}
		}

		for (var i in settings) {
			options[i] = settings[i];
		}

		options.start(target);

		if (target.nodeName.toLowerCase() !== "img") {
			options.error(target, 1, 'This is not an image.');
			options.complete(target, []);
			return [];
		}

		function resizeCanvas(image, canvas) {
			canvas.width = image.offsetWidth;
			canvas.height = image.offsetHeight;
		}

		// Grayscale function by Liu Liu
		function grayscale(image) {
			var canvas = document.createElement("canvas");
			var ctx = canvas.getContext("2d");

			canvas.width = image.offsetWidth;
			canvas.height = image.offsetHeight;

			ctx.drawImage(image, 0, 0);
			var imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
			var data = imageData.data;
			var pix1, pix2, pix = canvas.width * canvas.height * 4;
			while (pix > 0) {
				data[pix -= 4] = data[ pix1 = pix + 1] = data[ pix2 = pix + 2] = (data[pix] * 0.3 + data[pix1] * 0.59 + data[pix2] * 0.11);
			}
			ctx.putImageData(imageData, 0, 0);
			return canvas;
		}

		function detect() {
			try {
				var coords = ccv.detect_objects(grayscale(target), cascade, 5, 1);
			} catch(e) {
				options.error(target, 2, 'This image is not valid');
				return [];
			}

			var newCoords = [];

			for (var i = 0; i < coords.length; i++) {
				if (options.confidence == null || coords[i].confidence >= options.confidence) {
					newCoords.push({
						x : Math.round(coords[i].x),
						y : Math.round(coords[i].y),
						width : Math.round(coords[i].width),
						height : Math.round(coords[i].height),
						confidence : coords[i].confidence,
						neighbour : coords[i].neighbour
					});
				}
			}
			return newCoords;
		}

		var coords = detect();
		options.complete(target, coords);

		return coords;
	};

	return facedetection;

});