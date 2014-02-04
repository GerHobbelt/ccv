Face detection AMD library
============================

Based on the work of (Jay Salvat)[http://facedetection.jaysalvat.com/].
This library uses an algorithm by Liu Liu.

(Download release build)[https://github.com/sprky0/facedetection/blob/master/dist/facedetection.js]

Usage
--------
var results = facedetection(image, {options})

Settings
--------

**confidence:** Minimum level of confidence

**start:** Callback function trigged just before the process starts. **DOES'NT WORK PROPERLY**

	start:function(img) {
		// ...
	}

**complete:** Callback function trigged after the detection is completed

	complete:function(img, coords) {
		// ...
	}

**error:** Callback function trigged on errors

	error:function(img, code, message) {
		// ...
	}

Results
-------

Returns an array with found faces object:

**x:** Y coord of the face

**y:** Y coord of the face

**width:** Width of the face

**height:** Height of the face

**confidence:** Level of confidence
